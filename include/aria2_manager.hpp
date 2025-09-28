#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace aria2 {

void launch_aria2_handoff(const std::string& links_file);

bool is_rpc_running();

bool launch_aria2_daemon();

std::optional<std::string> rpc_add_download(const std::string& link);

std::optional<json> rpc_get_status(const std::string& gid);

bool rpc_remove_download(const std::string& gid);

void shutdown();

} // namespace aria2
