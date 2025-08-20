#include "shutdown_handler.hpp"
#include <atomic>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

std::atomic<bool> shutdown_handler::shutdown_requested{false};

#ifdef _WIN32
BOOL WINAPI shutdown_handler::console_handler(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
    shutdown_handler::shutdown_requested = true;
    return TRUE;
  }
  return FALSE;
}

void shutdown_handler::register_handler() {
  SetConsoleCtrlHandler(console_handler, TRUE);
}
#else
void shutdown_handler::handle_signal(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    std::cerr << "\nShutdown requested, beginning clean up...\n";
    shutdown_requested = true;
  }
}

void shutdown_handler::register_handler() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
}
#endif
