#pragma once

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <tuple>

namespace util {

[[noreturn]] inline void fatal_error(const std::string& message) {
  std::cerr << "Error: " << message << std::endl;
  throw std::runtime_error(message);
}

[[noreturn]] inline void fatal_exit(const std::string& message, int exit_code = 1) {
  std::cerr << "Fatal: " << message << std::endl;
  std::exit(exit_code);
}

bool validate_magnet_link(const std::string& magnet);

inline void set_env_variable(const std::string& key, const std::string& value) {
#ifdef _WIN32
  _putenv_s(key.c_str(), value.c_str());
#else
  setenv(key.c_str(), value.c_str(), 1);
#endif
}

void load_env_file(const std::string& path);

std::string get_rd_token(std::string& cli_token);

std::tuple<std::string, std::string, bool, std::string, bool> parse_arguments(int argc, char* argv[]);

// The following class helps with parallelization of link unrestriction
class TokenBucket {
public:
  // Constructor
  TokenBucket(size_t capacity, double refill_rate_per_sec);

  // Consume token
  void consume();

private:
  // Refill bucket
  void refill();

  size_t capacity;
  size_t tokens;
  double refill_rate;
  std::chrono::steady_clock::time_point last_refill;
  std::mutex bucket_mtx;
  std::condition_variable cv;
};

class File {
public:
  explicit File(std::string path);

  ~File();

  void keep_file();

  std::string get_path() const {
    return path;
  }

  void append_file_name_to_path(std::string& file_name, std::string& custom_path);

  bool create_text_file(const std::vector<std::string>& links);

private:
  std::string path;
  bool active; // whether we delete it in destructor
};

bool remove_file(const std::string& file_path);

// struct FileDownloadProgress {
//   std::string name;
//   float progress{0.0f};
// };

class FileDownloadProgress {
public:
  explicit FileDownloadProgress(std::string gid, std::string name);

  ~FileDownloadProgress();

  std::string get_name() const {
    return name;
  }

  float get_progress() const {
    return progress;
  }

  std::string get_gid() const {
    return gid;
  }

  void set_progress(float progress) {
    this->progress = progress;
  }

private:
  const std::string gid;
  const std::string name;
  float progress{0.0f};
}

void print_progress_bar(const std::vector<FileDownloadProgress>& files, size_t bar_width = 40);

} // namespace util
