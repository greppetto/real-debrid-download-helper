#include "tui.hpp"
#include "util.hpp"
#include <print>

int main() {
  auto magnet = tui::prompt_for_magnet_link();

  if (!util::validate_magnet_link(magnet)) {
    tui::show_status("Invalid magnet link!");
    return 1;
  }
}
