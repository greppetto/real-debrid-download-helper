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
  constexpr std::string default_output_path{"/tmp/"};
  const std::string my_token{util::get_rd_token()};
  api::RealDebridClient client{my_token};
  AppState state = AppState::ParseArguments;

  std::string magnet{};
  bool links_flag;
  std::string output_path{};
  bool aria2_flag;

  std::vector<api::Torrent> torrents;
  aria2::aria2Manager aria2_manager;

  // Process loop
  while (state != AppState::Finished && state != AppState::Error) {
    switch (state) {
    case AppState::ParseArguments: {
      std::tie(magnet, links_flag, output_path, aria2_flag) = util::parse_arguments(argc, argv);
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
      if (client.wait_for_status(torrent.id, "downloaded", torrent.size)) {
        torrent.links = client.get_download_links(torrent.links);
        if (output_path.empty()) {
          output_path = std::move(default_output_path);
        } else if (output_path.back() != '/') {
          output_path += '/';
        }
        output_path += (torrent.name + "_links.txt");
        std::println("Output file: {}", output_path);
        if (util::create_text_file(torrent.links, output_path)) {
          state = AppState::DownloadFiles;
          break;
        }
      }
      std::cerr << "Timed out waiting for status: downloaded." << std::endl;
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
      if (aria2_flag) {
        try {
          aria2_manager.launch_aria2(output_path);
        } catch (const std::exception& e) {
          util::fatal_exit(e.what());
        }
      }
      state = AppState::Finished;
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
    std::println("Error encountered. Please try again.");
    return 1;
  }
}
