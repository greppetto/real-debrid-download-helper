#include "util.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
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

util::TokenBucket::TokenBucket(size_t capacity, double refill_rate_per_sec)
    : capacity(capacity), tokens(capacity), refill_rate(refill_rate_per_sec), last_refill(std::chrono::steady_clock::now()) {}

void util::TokenBucket::consume() {
  std::unique_lock<std::mutex> lock(bucket_mtx);
  refill();
  cv.wait(lock, [this] { return tokens > 0; });
  --tokens;
}

void util::TokenBucket::refill() {
  auto time_now = std::chrono::steady_clock::now();
  double seconds_passed = std::chrono::duration<double>(time_now - last_refill).count();
  size_t new_tokens = static_cast<size_t>(seconds_passed * refill_rate);
  if (new_tokens > 0) {
    tokens = std::min(capacity, tokens + new_tokens);
    last_refill = time_now;
    cv.notify_all();
  }
}
