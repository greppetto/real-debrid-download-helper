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

  std::vector<util::FileDownloadProgress> files;

  // Process loop
  while (!shutdown_handler::shutdown_requested && state != AppState::Finished && state != AppState::Error) {
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
        std::println("\nDownload link(s):");
        for (auto& link : torrent.links) {
          std::println("{}", link);
        }
        state = AppState::Finished;
      }
      if (aria2_flag) {
        if (aria2::launch_aria2_daemon()) {
          try {
            std::println("\nSuccessfully started aria2 daemon.\n");
            auto& torrent = torrents.back();
            // if (torrent.links.size() == 1) {
            //   files.emplace_back(util::FileDownloadProgress(torrent.links.back(), torrent.files.));
            // } else {
            //   // assert(torrent.links.size() == torrent.files.size());
            //   for (auto&& [link, file] : std::views::zip(torrent.links, torrent.files)) {
            //     files.emplace_back(util::FileDownloadProgress(link, file));
            //   }
            // }
            for (auto&& [link, file] : std::views::zip(torrent.links, torrent.files)) {
              files.emplace_back(util::FileDownloadProgress(link, file));
            }
            if (state != AppState::Error) {
              state = AppState::MonitorDownloads;
            }
          } catch (const std::exception& e) {
            std::println("Skipping downloads...");
            state = AppState::Error;
          }
        } else {
          std::cerr << "Timed out waiting for aria2 daemon to start." << std::endl;
          state = AppState::Error;
        }
      }
      break;
    }

    case AppState::MonitorDownloads: {
      size_t total_downloads{files.size()};
      size_t completed_downloads{0};
      long total_size{0};
      float downloaded_size{0};

      if (total_downloads > 10) {
        for (auto& file : files) {
          if (auto parsed_response = aria2::rpc_get_status(file.get_gid())) {
            auto& parsed_json = (*parsed_response);
            if (parsed_json.contains("result")) {
              if (const auto& total_individual_length = std::stol(parsed_json["result"]["totalLength"].get<std::string>());
                  total_individual_length != 0) {
                total_size += total_individual_length;
              }
            }
          }
        }
      }

      while (!shutdown_handler::shutdown_requested) {
        for (auto& file : files) {
          if (auto parsed_response = aria2::rpc_get_status(file.get_gid())) {
            auto& parsed_json = (*parsed_response);
            if (parsed_json.contains("result")) {
              if (const auto& total_individual_length = std::stol(parsed_json["result"]["totalLength"].get<std::string>());
                  total_individual_length != 0) {
                auto current_individual_length = std::stof(parsed_json["result"]["completedLength"].get<std::string>());
                file.set_progress(current_individual_length / total_individual_length);
                downloaded_size += current_individual_length;
                if (file.get_progress() >= 1.0f) {
                  file.mark_completed();
                  completed_downloads += 1;
                }
              }
            }
          }
        }

        if (total_downloads > 10) {
          std::print("\r{} of {} files completed ({}%)", completed_downloads, total_downloads, (downloaded_size / total_size) * 100);
          std::fflush(stdout);
          downloaded_size = 0.0;
        } else {
          util::print_progress_bar(files);
          std::print("\033[{}A", files.size());
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        if (static_cast<long>(files.size()) ==
            std::ranges::count_if(files, [](const util::FileDownloadProgress& file) { return file.get_completion_status(); })) {
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
  }
  if (shutdown_handler::shutdown_requested) {
    std::println("\nProcess terminated gracefully and successfully.");
    return 1;
  } else if (state == AppState::Finished) {
    std::println("\nProcess executed successfully.");
    return 0;
  } else if (state == AppState::Error) {
    std::println("Something wrong went unconsidered. Please report the problem as descriptively as possible on GitHub.");
    std::println("https://github.com/greppetto/real-debrid-download-helper");
    return 1;
  }
}
