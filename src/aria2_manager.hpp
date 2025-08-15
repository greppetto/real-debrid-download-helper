#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace aria2 {

class aria2Manager {
public:
  inline void start_aria2_daemon() {
    std::string cmd = "aria2c --enable-rpc --rpc-secret=nuclearlaunchcode --rpc-listen-all=true --daemon=true";
    std::system(cmd.c_str());
  }

  bool start_download_and_exit(const std::vector<std::string>& links, const std::string& download_dir);

  std::optional<std::string> add_download(const std::string& link);

  std::optional<json> get_status(const std::string& gid);

  void shutdown();
};

} // namespace aria2
