#include "tui.hpp"
#include <iostream>
#include <print>
#include <string>

std::string tui::prompt_for_magnet_link() {
  std::string magnet;

  std::println("Enter magnet link:");
  std::getline(std::cin, magnet);
#ifndef NDEBUG
  magnet = "magnet:?xt=urn:btih:RIMVO75V62IJODFEHJL76EARVYQCERFY&dn=ubuntu-25."
           "04-desktop-amd64.iso&xl=6278520832&tr=https%3A%2F%2Ftorrent.ubuntu."
           "com%2Fannounce";
#endif
  std::println("You entered: {}", magnet);
  return magnet;
}

void tui::show_status(const std::string& msg) {
  std::println("{}", msg);
}
