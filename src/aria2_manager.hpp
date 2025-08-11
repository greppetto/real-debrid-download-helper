#pragma once

#include <string>
#include <vector>

namespace aria2 {

class aria2Manager {
public:
  void download(const std::vector<std::string>& urls);
  void shutdown();
};

} // namespace aria2
