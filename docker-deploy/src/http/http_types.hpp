#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct HttpRequest {
  std::string method;
  std::string host;
  std::string path;
};

struct HttpResponse {
  int status_code = 0;
  std::unordered_map<std::string, std::string> headers;
  std::vector<uint8_t> raw;
  std::string status_line;
};
