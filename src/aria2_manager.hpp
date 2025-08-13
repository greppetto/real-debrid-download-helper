#pragma once

#include <string>
#include <vector>

namespace aria2 {

class aria2Manager {
public:
  void download_and_exit(const std::vector<std::string>& links);
  bool launch(const std::vector<std::string>& links, const std::string& download_dir);
  void shutdown();
};

} // namespace aria2
