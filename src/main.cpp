#include "api.hpp"
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
      }
      state = AppState::WaitForConversion;
      break;

    case AppState::WaitForConversion:
      if (client.wait_for_status(torrents.back().id, "downloaded")) {
        client.get_download_links(torrents.back().links);
      }
      state = AppState::Finished;
    }
  }
}
