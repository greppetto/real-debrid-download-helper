#include "api.hpp"
#include "aria2_manager.hpp"
#include "shutdown_handler.hpp"
#include "util.hpp"
#include <cassert>
#include <optional>
#include <print>
#include <ranges>
#include <thread>

enum class AppState { ValidateMagnet, SendToAPI, WaitForConversion, DownloadFiles, MonitorDownloads, Finished, Error };

int main(int argc, char* argv[]) {
  shutdown_handler::register_handler();

  constexpr std::string default_output_path{"/tmp/"};
  util::File links_file{default_output_path};
  AppState state = AppState::ValidateMagnet;

  std::string api_token{};
  std::string magnet{};
  bool links_flag;
  std::string output_path{};
  bool aria2_flag;

  std::tie(api_token, magnet, links_flag, output_path, aria2_flag) = util::parse_arguments(argc, argv);

  api_token = util::get_rd_token(api_token);
  api::RealDebridClient client{api_token};

  std::vector<api::Torrent> torrents;
  aria2::aria2Manager aria2_manager;

  std::vector<std::string> download_gids;

  // Process loop
  while (state != AppState::Finished && state != AppState::Error) {
    switch (state) {
    case AppState::ValidateMagnet: {
      if (!util::validate_magnet_link(magnet)) {
        std::cerr << "Invalid magnet link." << std::endl;
        state = AppState::Error;
      } else {
        state = AppState::SendToAPI;
      }
      break;
    }

    case AppState::SendToAPI: {
      // Check if torrent has been sucessfully sent to Real-Debrid
      // Place it into the vector if so
      if (auto torrent = client.send_magnet_link(magnet)) {
        torrents.emplace_back(*torrent);
      } else {
        state = AppState::Error;
      }
      if (!links_flag && !aria2_flag) {
        state = AppState::Finished;
      } else {
        state = AppState::WaitForConversion;
      }
      break;
    }

    case AppState::WaitForConversion: {
      auto& torrent = torrents.back();
      if (client.wait_for_status(torrent.id, "downloaded", torrent.size)) {
        std::println("Caching complete! Obtaining unrestricted download links...");
        torrent.links = client.get_download_links(torrent.links);
        links_file.append_file_name_to_path(torrent.name, output_path);
        if (links_file.create_text_file(torrent.links)) {
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
        // assert(torrent.files.size() == torrent.links.size());
        // BUG: files vector and links vector can be of different sizes
        std::println("\nDownload links:");
        for (auto&& [name, link] : std::views::zip(torrent.files, torrent.links)) {
          std::println("{}: {}", name, link);
        }
      }
      if (aria2_flag) {
        try {
          // links_file.keep_file();
          // aria2_manager.launch_aria2_handoff(links_file.get_path());
          if (aria2_manager.launch_aria2_daemon()) {
            std::println("Successfully started aria2 daemon.");
            auto& torrent = torrents.back();
            for (const auto& link : torrent.links) {
              if (auto gid = aria2_manager.rpc_add_download(link)) {
                download_gids.push_back(std::move(*gid));
              } else {
                std::println("Failed to start download.");
                state = AppState::Error;
              }
            }
            state = AppState::MonitorDownloads;
          } else {
            std::cerr << "Timed out waiting for aria2 daemon to start." << std::endl;
            state = AppState::Error;
          }
        } catch (const std::exception& e) {
          util::fatal_exit(e.what());
        }
      }
      break;
    }

    case AppState::MonitorDownloads: {
      for (size_t i = 0; i < 5; ++i) {
        for (const auto& gid : download_gids) {
          if (auto response = aria2_manager.rpc_get_status(gid)) {
            std::println("{}", (*response).dump());
          }
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
      }
      state = AppState::Finished;
      break;
    }

    default:
      break;
    }

    if (shutdown_handler::shutdown_requested) {
      break;
    }
  }
  if (state == AppState::Finished) {
    std::println("\nProcess executed successfully.");
    return 0;
  } else if (state == AppState::Error) {
    std::println("Please try again.");
    return 1;
  }
  std::println("\nProcess terminated gracefully and successfully.");
}
