#include "util.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

bool util::validate_magnet_link(const std::string& magnet) {
  constexpr std::string_view prefix = "magnet:?xt=urn:btih:";
  if (!magnet.starts_with(prefix))
    return false;
  auto hash = magnet.substr(prefix.size(), 40);      // hex length
  return hash.length() == 40 || hash.length() == 32; // allow base32
}

void util::load_env_file(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "[INFO] No .env file found, skipping...\n";
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Move on if line is a comment, or lacks an '=' sign
    if (line.empty() || line[0] == '#')
      continue;
    auto pos = line.find('=');
    if (pos == std::string::npos)
      continue;

    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);

    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t\r\n"));
    key.erase(key.find_last_not_of(" \t\r\n") + 1);
    value.erase(0, value.find_first_not_of(" \t\r\n"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);

    set_env_variable(key, value);
  }
}
