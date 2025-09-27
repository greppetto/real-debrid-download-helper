#include "api.hpp"
#include "aria2_manager.hpp"
#include "shutdown_handler.hpp"
#include "util.hpp"
#include <cassert>
#include <iostream>
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
        // BUG: files vector and links vector can be of different sizes
        std::println("\nDownload link(s):");
        if (torrent.links.size() == 1) {
          std::println("{}: {}\n", torrent.name, torrent.links[0]);
        } else {
          assert(torrent.files.size() == torrent.links.size());
          for (auto&& [name, link] : std::views::zip(torrent.files, torrent.links)) {
            std::println("{}: {}\n", name, link);
          }
        }
        state = AppState::Finished;
      }
      if (aria2_flag) {
        try {
          // links_file.keep_file();
          // aria2_manager.launch_aria2_handoff(links_file.get_path());
          if (aria2_manager.launch_aria2_daemon()) {
            std::println("Successfully started aria2 daemon.\n");
            auto& torrent = torrents.back();
            for (const auto& link : torrent.links) {
              if (auto gid = aria2_manager.rpc_add_download(link)) {
                download_gids.push_back(std::move(*gid));
              } else {
                std::println("Failed to start download.");
                state = AppState::Error;
                break;
              }
            }
            if (state != AppState::Error) {
              state = AppState::MonitorDownloads;
            }
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
      // std::this_thread::sleep_for(std::chrono::seconds(5));
      auto& torrent = torrents.back();
      std::vector<util::FileDownloadProgress> files;
      if (torrent.links.size() == 1) {
        files.push_back({torrent.name});
      } else {
        assert(torrent.files.size() == torrent.links.size());
        for (const auto& file_name : torrent.files) {
          files.push_back({file_name});
        }
      }

      for (auto& file : files) {
        std::println("{}:{}", file.name, file.progress);
      }

      while (true) {
        if (shutdown_handler::shutdown_requested) {
          break;
        }
        assert(files.size() == download_gids.size());
        for (auto&& [gid, file] : std::views::zip(download_gids, files)) {
          if (auto parsed_response = aria2_manager.rpc_get_status(gid)) {
            auto& parsed_json = (*parsed_response);
            if (parsed_json.contains("result")) {
              if (const auto& total_length = std::stol(parsed_json["result"]["totalLength"].get<std::string>()); total_length != 0) {
                file.progress = std::stof(parsed_json["result"]["completedLength"].get<std::string>()) / total_length;
              }
            }
          }
        }

        std::print("\033[{}A", download_gids.size());
        util::print_progress_bar(files);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        bool completion_flag = true;
        for (auto& file : files) {
          if (file.progress < 1.0f) {
            completion_flag = false;
          }
        }
        if (completion_flag) {
          std::println("Download(s) complete!");
          break;
        }
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
