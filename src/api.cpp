#include "api.hpp"
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
    std::println("HTTP error {}: {}", response.status_code, response.error.message);
    return std::nullopt;
  }
  if (response.text.empty()) {
    return std::nullopt;
  }

  json parsed_response;
  try {
    parsed_response = json::parse(response.text);
  } catch (const json::parse_error& e) {
    std::println("JSON parse error: {}", e.what());
    return std::nullopt;
  }
  if (parsed_response.contains("error")) {
    std::println("API error: {}", parsed_response["error"].get<std::string>());
    return std::nullopt;
  }
  return parsed_response;
}

std::optional<api::Torrent> api::RealDebridClient::send_magnet_link(const std::string& magnet) const {
  if (auto parsed_response = request_json(HTTPMethod::POST, "/torrents/addMagnet", cpr::Payload{{"magnet", magnet}})) {
    std::string generated_id = (*parsed_response)["id"].get<std::string>();
    std::println("Generated ID: {}", generated_id);
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
            std::string file_name = parsed_json.contains("filename") && parsed_json["filename"].is_string()
                                        ? parsed_json["filename"].get<std::string>()
                                        : "Unknown filename";
            api::Torrent torrent{generated_id, file_name, std::vector<std::string>{files.begin(), files.end()},
                                 std::vector<std::string>{links.begin(), links.end()}};
            return torrent;
          }
        }
      }
    }
  }
  return std::nullopt;
}

bool api::RealDebridClient::wait_for_status(const std::string& torrent_id, const std::string& desired_status, int max_retries) const {
  for (int i = 0; i < max_retries; ++i) {
    std::this_thread::sleep_for(poll_interval);
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
  }
  std::println("Timeout waiting for status: {}", desired_status);
  return false;
}

std::vector<std::string> api::RealDebridClient::get_download_links(const std::vector<std::string>& links) {
  std::vector<std::string> unrestricted_download_links;
  unrestricted_download_links.reserve(links.size());

  for (const auto& link : links) {
    std::println("Restricted link: {}", link);

    if (auto parsed_response = request_json(HTTPMethod::POST, "/unrestrict/link", cpr::Payload{{"link", link}})) {
      auto& parsed_json = (*parsed_response);
      if (parsed_json.contains("download") && parsed_json["download"].is_string()) {
        unrestricted_download_links.push_back(parsed_json["download"]);
        std::println("Unrestricted counterpart: {}", unrestricted_download_links.back());
      } else {
        std::println("Unexpected response format for link: {}", link);
      }
    } else {
      std::println("Failed to unrestrict link: {}", link);
    }
  }
  return unrestricted_download_links;
}
