#pragma once
#include "cache.hpp"
#include "../http/http_types.hpp"
#include <functional>
#include <vector>
#include <utility>
#include <string>
#include <ctime>
#include "../log/logger.hpp"

class GetPipeline {
 public:
  GetPipeline(Cache &cache, Logger &logger);

  HttpResponse handle(
    int id,
    const HttpRequest& req,
    std::time_t now,
    std::function<HttpResponse(
        const HttpRequest&,
        const std::vector<std::pair<std::string,std::string>>&
    )> fetcher);

 private:
  Cache &cache_;
  Logger &logger_;
};
