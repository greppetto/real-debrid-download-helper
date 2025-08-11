#include "aria2_manager.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

void aria2::aria2Manager::download(const std::vector<std::string>& links) {
  if (links.empty()) {
    std::println("No download links provided.");
    return;
  }

  // Build command line: aria2c <url1> <url2> ... --dir=downloads --continue=true
  // Customize options as needed
  std::stringstream cmd;
  cmd << "aria2c --dir=downloads --continue=true";

  for (const auto& link : links) {
    cmd << " \"" << link << "\"";
  }

  std::println("Running command: {}", cmd.str());

  int output = std::system(cmd.str().c_str());
  if (output != 0) {
    std::println("aria2c exited with code {}.", output);
  }
}
