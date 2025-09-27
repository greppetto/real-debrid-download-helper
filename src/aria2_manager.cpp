#include "aria2_manager.hpp"
#include "util.hpp"
#include <chrono>
#include <cpr/cpr.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

void aria2::aria2Manager::launch_aria2_handoff(const std::string& links_file) {
  if (links_file.empty()) {
    std::cerr << "[aria2] No URLs provided.\n";
  }

#ifdef _WIN32
  // Build the aria2c command
  std::ostringstream cmd;
  cmd << "aria2c -i "
         " << links_file << -d ./Downloads -x 16 - s16 --continue=true --max-connection-per-server=4";
  std::string command = cmd.str();

  // On Windows: use CreateProcess to avoid blocking
  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);

  // Start the child process
  if (!CreateProcessA(nullptr,            // No module name (use command line)
                     command.data(),      // Command line, mutable C-string
                     nullptr,             // Process handle not inheritable
                     nullptr,             // Thread handle not inheritable
                     FALSE,               // Set handle inheritance to FALSE
                     CREATE_NEW_CONSOLE,  // Creation flags
                     nullptr,             // Use parent's environment block
                     nullptr,             // Use parent's starting directory
                     &si,                 // Pointer to STARTUPINFO structure
                     &pi)                 // Pointer to PROCESS_INFORMATION structure) 
  {
    util::fatal_exit("[aria2] Failed to launch process. Error: " + static_cast<std::string>(GetLastError()) + "\n");
    return;
  }

  // aria2c runs independently
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
#else
  // On Linux/macOS: fork + exec
  pid_t pid = fork();
  if (pid == 0) {
    // Child process: detach from terminal
    if (setsid() < 0) {
      _exit(1); // exec failed
    }

    // Prepare arguments
    char* args[] = {(char*)"aria2c", (char*)"-i",    (char*)links_file.c_str(), (char*)"-d ./Downloads",
                    (char*)"-x 16",  (char*)"-s 16", (char*)"--continue=true",  (char*)"--max-connection-per-server=4",
                    nullptr};

    execvp("aria2c", args);
    // If execvp returns, there was an error
    _exit(1); // exec failed
  } else if (pid < 0) {
    util::fatal_exit("[aria2] Failed to fork process.");
  }
  // aria2c runs independently
#endif
}

bool aria2::aria2Manager::is_rpc_running() {
  try {
    json payload = {{"jsonrpc", "2.0"}, {"id", "ping"}, {"method", "aria2.getVersion"}, {"params", {"token:nuclearlaunchcode"}}};

    cpr::Response response =
        cpr::Post(cpr::Url{"http://localhost:6800/jsonrpc"}, cpr::Body{payload.dump()}, cpr::Header{{"Content-Type", "application/json"}});

    if (response.status_code == 200) {
      auto parsed_json = json::parse(response.text);
      return parsed_json.contains("result"); // valid RPC reply
    }
  } catch (const std::exception& e) {
    std::cerr << "[aria2] RPC check failed: " << e.what() << "\n";
  }
  return false;
}

bool aria2::aria2Manager::launch_aria2_daemon() {
  std::string cmd = "aria2c --enable-rpc --rpc-secret=nuclearlaunchcode --rpc-listen-all=true --daemon=true";

  if (is_rpc_running()) {
    return true;
  }

  std::system(cmd.c_str());

  // Retry a few times until RPC is responsive
  for (size_t i = 0; i < 5; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (is_rpc_running()) {
      return true;
    }
  }

  return false;
}

std::optional<std::string> aria2::aria2Manager::rpc_add_download(const std::string& link) {
  json payload = {{"jsonrpc", "2.0"}, {"id", "JID"}, {"method", "aria2.addUri"}, {"params", {"token:nuclearlaunchcode", json::array({link})}}};

  cpr::Response response =
      cpr::Post(cpr::Url{"http://localhost:6800/jsonrpc"}, cpr::Body{payload.dump()}, cpr::Header{{"Content-Type", "application/json"}});

  if (response.status_code == 200) {
    auto parsed_json = json::parse(response.text);
    if (parsed_json.contains("result")) {
      return parsed_json["result"].get<std::string>();
    }
  }
  return std::nullopt;
}

std::optional<json> aria2::aria2Manager::rpc_get_status(const std::string& gid) {
  json payload = {
      {"jsonrpc", "2.0"},
      {"id", "JID"},
      {"method", "aria2.tellStatus"},
      {"params", {"token:nuclearlaunchcode", gid, json::array({"status", "totalLength", "completedLength", "downloadSpeed", "connections"})}}};

  cpr::Response response =
      cpr::Post(cpr::Url{"http://localhost:6800/jsonrpc"}, cpr::Body{payload.dump()}, cpr::Header{{"Content-Type", "application/json"}});

  if (response.status_code == 200) {
    auto parsed_json = json::parse(response.text);
    return parsed_json;
  }
  return std::nullopt;
}

bool aria2::aria2Manager::rpc_remove_download(const std::string& gid) {
  json payload = {{"jsonrpc", "2.0"}, {"id", "JID"}, {"method", "aria2.remove"}, {"params", {"token:nuclearlaunchcode", gid}}};

  cpr::Response response =
      cpr::Post(cpr::Url{"http://localhost:6800/jsonrpc"}, cpr::Body{payload.dump()}, cpr::Header{{"Content-Type", "application/json"}});

  if (response.status_code == 200) {
    auto parsed_json = json::parse(response.text);
    if (parsed_json.get<std::string>() == gid) {
      return true;
    }
  }
  return false;
}
