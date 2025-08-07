#include "tui.hpp"
#include <iostream>
#include <print>
#include <string>

std::string tui::prompt_for_magnet_link() {
  std::string magnet;

  std::println("Enter magnet link:");
  std::getline(std::cin, magnet);
  std::println("You entered: {}", magnet);

  return magnet;
}

void tui::show_status(const std::string& msg) {
  std::println("{}", msg);
}
