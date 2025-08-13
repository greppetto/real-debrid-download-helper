#include "api.hpp"
#include "aria2_manager.hpp"
#include "tui.hpp"
#include "util.hpp"
#include <optional>
#include <print>

enum class AppState { PromptMagnet, ValidateMagnet, SendToAPI, WaitForConversion, DownloadFiles, Finished, Error };

int main() {
  const std::string my_token{util::get_rd_token()};
  std::println("Token: {}", my_token);
  api::RealDebridClient client{my_token};
  AppState state = AppState::PromptMagnet;
  std::string magnet{};
  std::vector<api::Torrent> torrents;
  aria2::aria2Manager aria2_manager;

  while (state != AppState::Finished && state != AppState::Error) {
    switch (state) {
    case AppState::PromptMagnet:
      magnet = tui::prompt_for_magnet_link();
      state = AppState::ValidateMagnet;
      break;

    case AppState::ValidateMagnet:
      if (!util::validate_magnet_link(magnet)) {
        tui::show_status("Invalid magnet link. Try again!");
        state = AppState::Error;
      } else {
        state = AppState::SendToAPI;
      }
      break;

    case AppState::SendToAPI:
      // Check if torrent sucessfully sent to Real-Debrid
      // Place it into the vector if so
      if (auto torrent = client.send_magnet_link(magnet)) {
        torrents.emplace_back(*torrent);
      } else {
        state = AppState::Error;
      }
      state = AppState::WaitForConversion;
      break;

    case AppState::WaitForConversion:
      if (client.wait_for_status(torrents.back().id, "downloaded")) {
        torrents.back().links = client.get_download_links(torrents.back().links);
        state = AppState::DownloadFiles;
      } else {
        state = AppState::Error;
      }
      break;

    case AppState::DownloadFiles:
      aria2_manager.launch(torrents.back().links, ".");
      state = AppState::Finished;
      break;

    case AppState::Finished:
      std::println("Process complete.");
      break;

    case AppState::Error:
      std::println("Encountered an unknown error. Please try again.");
      break;
    }
  }
}
