#include "api.hpp"
#include "aria2_manager.hpp"
#include "tui.hpp"
#include "util.hpp"
#include <optional>
#include <print>
#include <thread>

enum class AppState { PromptMagnet, ValidateMagnet, SendToAPI, WaitForConversion, DownloadFiles, Finished, Error };

int main() {
  const std::string my_token{util::get_rd_token()};
  std::println("Token: {}", my_token);
  api::RealDebridClient client{my_token};
  AppState state = AppState::PromptMagnet;
  std::string magnet{};
  std::vector<api::Torrent> torrents;
  aria2::aria2Manager aria2_manager;
  aria2_manager.start_aria2_daemon();

  while (state != AppState::Finished && state != AppState::Error) {
    switch (state) {
    case AppState::PromptMagnet: {
      magnet = tui::prompt_for_magnet_link();
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
      if (client.wait_for_status(torrents.back().id, "downloaded")) {
        torrents.back().links = client.get_download_links(torrents.back().links);
        state = AppState::DownloadFiles;
      } else {
        state = AppState::Error;
      }
      break;
    }

    case AppState::DownloadFiles: {
      auto& torrent = torrents.back();
      for (auto& link : torrent.links) {
        if (auto gid = aria2_manager.add_download(link)) {
          link = std::move(*gid);
          std::println("GID: {}", link);
          // Let download start
          std::this_thread::sleep_for(std::chrono::seconds(10));
          if (const auto parsed_response = aria2_manager.get_status(link)) {
            auto& parsed_json = (*parsed_response);
            if (parsed_json.contains("result")) {
              std::println("Status: {}", parsed_json["result"]["status"].get<std::string>());
              std::println("Completed Length: {}", parsed_json["result"]["completedLength"].get<std::string>());
              std::println("Total Length: {}", parsed_json["result"]["totalLength"].get<std::string>());
              std::println("Download Speed: {}", parsed_json["result"]["downloadSpeed"].get<std::string>());
              std::println("Connections: {}", parsed_json["result"]["connections"].get<std::string>());
            }
          }
        }
      }
      // aria2_manager.start_download_and_exit(torrents.back().links, ".");
      state = AppState::Finished;
      break;
    }

    default:
      break;
    }
  }
  if (state == AppState::Finished) {
    std::println("Process complete.");
  } else if (state == AppState::Error) {
    std::println("Encountered an unknown error. Please try again.");
  }
}
