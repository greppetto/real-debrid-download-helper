#include "util.h"
#include "util.hpp"
#include <string>

bool util::validate_magnet_link(const std::string& magnet) {
  constexpr std::string_view prefix = "magnet:?xt=urn:btih:";
  if (!magnet.starts_with(prefix))
    return false;
  auto hash = magnet.substr(prefix.size(), 40);      // hex length
  return hash.length() == 40 || hash.length() == 32; // allow base32
}
