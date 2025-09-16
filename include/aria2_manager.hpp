#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace aria2 {

class aria2Manager {
public:
  inline void launch_aria2_daemon() {
    std::string cmd = "aria2c --enable-rpc --rpc-secret=nuclearlaunchcode --rpc-listen-all=true --daemon=true";
    std::system(cmd.c_str());
  }

  void launch_aria2_handoff(const std::string& links_file);

  std::optional<std::string> rpc_add_download(const std::string& link);

  std::optional<json> rpc_get_status(const std::string& gid);

  void shutdown();
};

} // namespace aria2
