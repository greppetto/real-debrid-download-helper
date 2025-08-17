#pragma once

#include <chrono>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace api {

struct Torrent {
  std::string id;
  std::string file_name;
  std::vector<std::string> files;
  std::vector<std::string> links;
};

enum class HTTPMethod { GET, POST, PUT, DELETE };

constexpr std::chrono::seconds poll_interval{2};
constexpr std::chrono::seconds post_delay{1};

class RealDebridClient {
public:
  explicit RealDebridClient(std::string token);

  // Sends magnet link, returns an optional (Torrent object)
  std::optional<Torrent> send_magnet_link(const std::string& magnet) const;

  // Polls Real-Debrid to check if the torrent is ready for download
  bool wait_for_status(const std::string& torrent_id, const std::string& desired_status, int max_retries = 30) const;

  // Gets a list of download URLs from Real-Debrid
  std::vector<std::string> get_download_links(const std::vector<std::string>& links);

private:
  static inline const std::string url{"https://api.real-debrid.com/rest/1.0"};
  std::string token;
  cpr::Bearer bearer;
  // Sends GET or POST request, parses response, and returns json object
  std::optional<nlohmann::json> request_json(HTTPMethod method, const std::string& url_suffix, const cpr::Payload& payload = {}) const;
};

} // namespace api
