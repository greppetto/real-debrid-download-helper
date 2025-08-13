#include "aria2_manager.hpp"
#include <iostream>
#include <sstream>
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

void aria2::aria2Manager::download_and_exit(const std::vector<std::string>& links) {
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

bool aria2::aria2Manager::launch(const std::vector<std::string>& links, const std::string& download_dir) {
  if (links.empty()) {
    std::cerr << "[Aria2] No URLs provided.\n";
    return false;
  }

  // Build the aria2c command
  std::ostringstream cmd;
  cmd << "aria2c --dir=\"" << download_dir << "\" --continue=true --max-connection-per-server=4";
  for (const auto& link : links) {
    cmd << " \"" << link << "\"";
  }

  std::string command = cmd.str();
  std::cout << "[Aria2] Launching: " << command << "\n";

#ifdef _WIN32
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
    std::cerr << "[Aria2] Failed to launch process. Error: " << GetLastError() << "\n";
    return;
  }

  // aria2c runs independently
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
#else
  // On Linux/macOS: fork + exec
  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    // The function call "execl()" initiates a new program in the same environment in which it is operating
    // The list of arguments is terminated by NULL
    execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
    _exit(127); // exec failed
  } else if (pid < 0) {
    std::cerr << "[Aria2] fork() failed.\n";
    return false;
  }
  // aria2c runs independently
#endif

  return true;
}
