#include "util.hpp"
#include "CLI11.hpp"
#include "aria2_manager.hpp"
#include "shutdown_handler.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

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

std::string util::get_rd_token(std::string& cli_token) {
  // 1. CLI token provided
  if (!cli_token.empty()) {
    std::ofstream env_file(".env", std::ios::trunc);
    if (!env_file) {
      throw std::runtime_error("Failed to write .env file.");
    }
    env_file << "REAL_DEBRID_API_TOKEN=" << cli_token << "\n";
    env_file.close();
    std::cerr << "API token saved to .env\n";
    return cli_token;
  }
  load_env_file(".env");
  // 2. Check environment variable
  if (const char* env_token = std::getenv("REAL_DEBRID_API_TOKEN")) {
    return std::string{env_token};
  } else {
    fatal_exit("No API token found; Run with -t <token> or set REAL_DEBRID_API_TOKEN environment variable.");
  }
}

std::tuple<std::string, std::string, bool, std::string, bool> util::parse_arguments(int argc, char* argv[]) {
  CLI::App app{"Real-Debrid → Aria2 helper"};

  std::string api_token{};
  std::string magnet{};
  bool links_flag = false;
  bool aria2_flag = false;
  std::string output_folder{};

  app.add_option("-t,--token", api_token, "Set API token and save it locally");
  app.add_option("-m,--magnet", magnet, "Magnet link")->required();
  app.add_flag("-l,--links", links_flag, "Print unrestricted links");
  app.add_option("-o,--output", output_folder, "Specify path for output .txt file");
  app.add_flag("-a,--aria2", aria2_flag, "Start download using aria2");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    std::exit(app.exit(e));
  }

  return {api_token, magnet, links_flag, output_folder, aria2_flag};
}

util::TokenBucket::TokenBucket(size_t capacity, double refill_rate_per_sec)
    : capacity(capacity), tokens(capacity), refill_rate(refill_rate_per_sec), last_refill(std::chrono::steady_clock::now()) {
  refill_thread = std::thread([this]() {
    while (!stop_flag) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      refill();
    }
  });
}

util::TokenBucket::~TokenBucket() {
  {
    std::lock_guard<std::mutex> lock(bucket_mtx);
    stop_flag = true;
  }
  cv.notify_all();
  if (refill_thread.joinable()) {
    refill_thread.join();
  }
}

void util::TokenBucket::consume() {
  std::unique_lock<std::mutex> lock(bucket_mtx);
  cv.wait(lock, [this] { return tokens > 0 || stop_flag; });
  if (stop_flag) {
    return;
  }
  --tokens;
}

void util::TokenBucket::refill() {
  auto time_now = std::chrono::steady_clock::now();
  double seconds_passed = std::chrono::duration<double>(time_now - last_refill).count();
  size_t new_tokens = static_cast<size_t>(seconds_passed * refill_rate);
  if (new_tokens > 0) {
    std::lock_guard<std::mutex> lock(bucket_mtx);
    tokens = std::min(capacity, tokens + new_tokens);
    last_refill = time_now;
    cv.notify_all();
  }
}

util::File::File(std::string path) : path(std::move(path)), active(true) {}

util::File::~File() {
  if (active) {
    if (path != "/tmp/" && std::remove(path.c_str()) != 0) {
      std::cerr << "Temp file " << path << " could not be removed.\n";
    }
  }
}

void util::File::keep_file() {
  active = false;
}

void util::File::append_file_name_to_path(std::string& file_name, std::string& custom_path) {
  if (custom_path.empty()) {
    path += file_name + "_links.txt";
  } else {
    keep_file();
    path = std::move(custom_path);
    if (path.back() != '/') {
      path += '/';
    }
    path += file_name + "_links.txt";
    std::println("\nOutput file: {}", path);
  }
}

bool util::File::create_text_file(const std::vector<std::string>& links) {
  // Open file in truncate mode
  std::ofstream file(path, std::ios::trunc);

  if (!file.is_open()) {
    std::cerr << "Error: Could not open file!\n";
    return false;
  }

  // Append links
  for (const auto& link : links) {
    file << link << '\n';
  }

  file.close();
  return true;
}

util::FileDownloadProgress::FileDownloadProgress(const std::string& link, const std::string& name)
    : gid{}, name(std::move(name)), progress{0.0f}, completion_status(false) {
  gid = aria2::rpc_add_download(link);
  if (!gid) {
    util::fatal_error("[aria2] Could not start download.\n");
  }
}

util::FileDownloadProgress::~FileDownloadProgress() {
  if (shutdown_handler::shutdown_requested && !completion_status) {
    try {
      aria2::rpc_remove_download(gid.value());
      // std::println("[aria2] Successfully stopped download {}.", name);
    } catch (const std::exception& e) {
      std::cerr << "[aria2] Could not remove the ongoing download: " << name << "\n";
    }
  }
}

void util::print_progress_bar(const std::vector<util::FileDownloadProgress>& files, size_t max_length, size_t bar_width) {
  if (files.empty()) {
    return;
  }
  for (const auto& file : files) {
    if (file.get_progress() != 0 && !file.get_completion_status()) {
      size_t current_position = static_cast<size_t>(bar_width * file.get_progress());

      // Print filename left-aligned with a width of max_length
      std::print("{:<{}}", file.get_name(), max_length);

      // Print progress bar
      std::print(" [");
      for (size_t i = 0; i < bar_width; ++i) {
        if (i < current_position) {
          std::print("=");
        } else if (i == current_position) {
          std::print(">");
        } else {
          std::print(" ");
        }
      }
      std::println("] {}%", static_cast<size_t>(file.get_progress() * 100.0));
    }
  }
}
