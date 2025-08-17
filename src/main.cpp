#include "api.hpp"
#include "aria2_manager.hpp"
#include "tui.hpp"
#include "util.hpp"
#include <cassert>
#include <optional>
#include <print>
#include <ranges>
#include <thread>

enum class AppState { ParseArguments, ValidateMagnet, SendToAPI, WaitForConversion, DownloadFiles, Finished, Error };

int main(int argc, char* argv[]) {
  constexpr std::string default_output_file{"/tmp/aria2_links.txt"};
  const std::string my_token{util::get_rd_token()};
  api::RealDebridClient client{my_token};
  AppState state = AppState::ParseArguments;
  std::string magnet{};
  bool links_flag;
  std::string output_file{};
  std::vector<api::Torrent> torrents;
  aria2::aria2Manager aria2_manager;
  // aria2_manager.start_aria2_daemon();

  while (state != AppState::Finished && state != AppState::Error) {
    switch (state) {
    case AppState::ParseArguments: {
      std::tie(magnet, links_flag, output_file) = util::parse_arguments(argc, argv);
#ifndef NDEBUG
      magnet = "magnet:?xt=urn:btih:RIMVO75V62IJODFEHJL76EARVYQCERFY&dn=ubuntu-25."
               "04-desktop-amd64.iso&xl=6278520832&tr=https%3A%2F%2Ftorrent.ubuntu."
               "com%2Fannounce";
#endif
      state = AppState::ValidateMagnet;
      break;
    }

    case AppState::ValidateMagnet: {
      if (!util::validate_magnet_link(magnet)) {
        tui::show_status("Invalid magnet link. Try again!");
        state = AppState::Error;
      } else {
        state = AppState::SendToAPI;
      }
      break;
    }

    case AppState::SendToAPI: {
      // Check if torrent sucessfully sent to Real-Debrid
      // Place it into the vector if so
      if (auto torrent = client.send_magnet_link(magnet)) {
        torrents.emplace_back(*torrent);
      } else {
        state = AppState::Error;
      }
      state = AppState::WaitForConversion;
      break;
    }

    case AppState::WaitForConversion: {
      auto& torrent = torrents.back();
      if (client.wait_for_status(torrent.id, "downloaded")) {
        torrent.links = client.get_download_links(torrent.links);
        if (output_file.empty()) {
          output_file = std::move(default_output_file);
        }
        if (util::create_text_file(torrent.links, output_file)) {
          state = AppState::DownloadFiles;
          break;
        }
      }
      state = AppState::Error;
      break;
    }

    case AppState::DownloadFiles: {
      if (links_flag) {
        auto& torrent = torrents.back();
        assert(torrent.files.size() == torrent.links.size());
        for (auto&& [name, link] : std::views::zip(torrent.files, torrent.links)) {
          std::println("{}: {}", name, link);
        }
      }
      try {
        aria2_manager.launch_aria2(output_file);
        state = AppState::Finished;
      } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
      }
      break;
    }

    default:
      break;
    }
  }
  if (state == AppState::Finished) {
    std::println("Process complete.");
    return 0;
  } else if (state == AppState::Error) {
    std::println("Encountered an unknown error. Please try again.");
    return 1;
  }
}
