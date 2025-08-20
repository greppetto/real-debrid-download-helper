#include <atomic>
#ifdef _WIN32
#include <windows.h>
#endif

namespace shutdown_handler {

extern std::atomic<bool> shutdown_requested;

#ifdef _WIN32
BOOL WINAPI console_handler(DWORD signal);
#else
void handle_signal(int signal);
#endif

void register_handler();

} // namespace shutdown_handler
