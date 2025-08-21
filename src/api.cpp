#include "api.hpp"
#include "shutdown_handler.hpp"
#include "util.hpp"
#include <chrono>
#include <cpr/cpr.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>

using json = nlohmann::json;

api::RealDebridClient::RealDebridClient(std::string token) : token{std::move(token)}, bearer{this->token} {}

std::optional<json> api::RealDebridClient::request_json(HTTPMethod method, const std::string& url_suffix, const cpr::Payload& payload) const {
  cpr::Response response;
  switch (method) {
  case HTTPMethod::GET:
    response = cpr::Get(cpr::Url{url + url_suffix}, bearer);
    break;
  case HTTPMethod::POST:
    response = cpr::Post(cpr::Url{url + url_suffix}, bearer, payload);
    break;
  case HTTPMethod::PUT:
    response = cpr::Put(cpr::Url{url + url_suffix}, bearer, payload);
    break;
  case HTTPMethod::DELETE:
    response = cpr::Delete(cpr::Url{url + url_suffix}, bearer);
    break;
  }

  if (response.status_code >= 400) {
    std::cerr << "HTTP error " << response.status_code << ": " << response.error.message << std::endl;
    return std::nullopt;
  }
  if (response.text.empty()) {
    return std::nullopt;
  }

  json parsed_response;
  try {
    parsed_response = json::parse(response.text);
  } catch (const json::parse_error& e) {
    std::cerr << "JSON parse error: " << e.what() << std::endl;
    return std::nullopt;
  }
  if (parsed_response.contains("error")) {
    std::cerr << "API error: " << parsed_response["error"].get<std::string>() << std::endl;
    return std::nullopt;
  }
  return parsed_response;
}

std::optional<api::Torrent> api::RealDebridClient::send_magnet_link(const std::string& magnet) const {
  std::println("Sending magnet link to Real-Debrid for caching...");
  if (auto parsed_response = request_json(HTTPMethod::POST, "/torrents/addMagnet", cpr::Payload{{"magnet", magnet}})) {
    std::string generated_id = (*parsed_response)["id"].get<std::string>();
    std::println("Successfully done! Generated ID: {}", generated_id);
    if (auto parsed_response = request_json(HTTPMethod::GET, "/torrents/info/" + generated_id)) {
      auto& parsed_json = (*parsed_response);
      if (parsed_json.contains("status") && parsed_json["status"].is_string() && wait_for_status(generated_id, "waiting_files_selection")) {
        std::this_thread::sleep_for(post_delay);

        // Select all files and start download
        request_json(HTTPMethod::POST, "/torrents/selectFiles/" + generated_id, cpr::Payload{{"files", "all"}});
        std::this_thread::sleep_for(post_delay);
        // Get updated torrent info
        if (auto parsed_response = request_json(HTTPMethod::GET, "/torrents/info/" + generated_id)) {
          auto& parsed_json = (*parsed_response);
          if (parsed_json.contains("files") && parsed_json.contains("links")) {
            auto files = parsed_json["files"] | std::views::transform([](const json& file) { return file["path"].get<std::string>(); });
            auto links = parsed_json["links"] | std::views::transform([](const json& link) { return link.get<std::string>(); });
            std::string torrent_name = parsed_json.contains("filename") && parsed_json["filename"].is_string()
                                           ? parsed_json["filename"].get<std::string>()
                                           : "Unknown filename";
            auto size = parsed_json.contains("bytes") ? parsed_json["bytes"].get<int>() : 0;
            api::Torrent torrent{generated_id, torrent_name, std::vector<std::string>{files.begin(), files.end()},
                                 std::vector<std::string>{links.begin(), links.end()}, size};
            return torrent;
          }
        }
      }
    }
  }
  return std::nullopt;
}

bool api::RealDebridClient::wait_for_status(const std::string& torrent_id, const std::string& desired_status, int torrent_size) const {
  int max_retries, initial_interval;
  if (torrent_size < 500 * 1024 * 1024) { // < 500MB
    max_retries = 30;                     // ~2.5 min
    initial_interval = 5;
  } else if (torrent_size < 5ULL * 1024 * 1024 * 1024) { // 500MB–5GB
    max_retries = 120;                                   // ~20 min
    initial_interval = 10;
  } else {             // > 5GB
    max_retries = 300; // ~2.5 hours
    initial_interval = 30;
  }
  int interval = initial_interval;
  int max_interval = 300;

  for (int i = 0; i < max_retries; ++i) {
    if (shutdown_handler::shutdown_requested) {
      break;
    }
    if (i % 10 == 0) {
      std::println("Waiting for torrent to be cached...");
    }
    if (auto parsed_response = request_json(HTTPMethod::GET, "/torrents/info/" + torrent_id)) {
      auto& parsed_json = (*parsed_response);
      std::string status =
          parsed_json.contains("status") && parsed_json["status"].is_string() ? parsed_json["status"].get<std::string>() : "status unknown";

      if (status == desired_status)
        return true;

      if (status == "error" || status == "magnet_error" || status == "virus" || status == "downloaded") {
        std::println("Torrent {} ended with status: {}", torrent_id, status);
        return false;
      }
    } else {
      return false;
    }

    std::this_thread::sleep_for(static_cast<std::chrono::seconds>(interval));
    if (interval < max_interval) {
      interval = std::min(interval * 2, max_interval);
    }
  }
  std::println("", desired_status);
  return false;
}

std::vector<std::string> api::RealDebridClient::get_download_links(const std::vector<std::string>& links) {
  std::mutex print_mutex;
  std::vector<std::string> unrestricted_download_links;
  unrestricted_download_links.reserve(links.size());

  // API restricted to 250 requests per minute
  util::TokenBucket bucket(4, 4.0);

  std::vector<std::future<std::optional<std::string>>> futures;

  for (const auto& link : links) {
    futures.push_back(std::async(std::launch::async, [&] {
      bucket.consume();

      if (auto parsed_response = request_json(HTTPMethod::POST, "/unrestrict/link", cpr::Payload{{"link", link}})) {
        auto& parsed_json = (*parsed_response);
        if (parsed_json.contains("download") && parsed_json["download"].is_string()) {
          std::string download_link = parsed_json["download"];
          {
            std::scoped_lock lock(print_mutex);
            // std::println("Unrestricted counterpart: {}", download_link);
          }
          return std::optional<std::string>{std::move(download_link)};
        } else {
          std::println("Unexpected response format for link: {}", link);
        }
      } else {
        std::println("Failed to unrestrict link: {}", link);
      }
      return std::optional<std::string>{};
    }));
  }

  for (auto& future : futures) {
    if (auto link = future.get()) {
      unrestricted_download_links.emplace_back(std::move(*link));
    }
  }

  return unrestricted_download_links;
}
