#include "get_pipeline.hpp"
#include <iostream>

GetPipeline::GetPipeline(Cache &cache, Logger &logger)
    : cache_(cache), logger_(logger) {}

static std::string extract_status_line(const std::vector<unsigned char>& raw) {
  std::string s(raw.begin(), raw.end());
  size_t pos = s.find("\r\n");
  if (pos == std::string::npos) {
    return "HTTP/1.1 200 OK";
  }
  return s.substr(0, pos);
}

static std::string response_line_for_log(const HttpResponse &resp) {
  if (!resp.status_line.empty()) {
    return resp.status_line;
  }
  if (resp.status_code > 0) {
    return "HTTP/1.1 " + std::to_string(resp.status_code);
  }
  return "UNKNOWN RESPONSE";
}

HttpResponse GetPipeline::handle(
    int id,
    const HttpRequest &req,
    std::time_t now,
    std::function<HttpResponse(
        const HttpRequest&,
        const std::vector<std::pair<std::string,std::string>>&
    )> fetcher) {

  std::string key = req.host + req.path;

  auto entry_opt = cache_.get(key);
  auto status = classify(entry_opt, now);

  if (status == CacheStatus::FRESH) {
    logger_.log(id, "in cache, valid");
    std::string status_line = extract_status_line(entry_opt->raw);
    return HttpResponse{200, {}, entry_opt->raw, status_line};
  }

  if (status == CacheStatus::MISS) {
    logger_.log(id, "not in cache");
    
    logger_.log(id,
    "Requesting \"" +
    req.method + " " +
    req.host + req.path +
    " HTTP/1.1\" from " +
    req.host);

    HttpResponse origin = fetcher(req, {});

    if (origin.status_code == 0) {
      logger_.log(id, "ERROR contacting origin server");
      return origin;
    }

    logger_.log(id,
    "Received \"" +
    response_line_for_log(origin) +
    "\" from " +
    req.host);

    std::string reason;
    auto new_entry = build_cache_entry(origin, now, reason);

    if (origin.status_code == 200) {
      if (!new_entry.has_value()) {
	logger_.log(id, "not cacheable because " + reason);
      } else {
	if (new_entry->requires_validation) {
	  logger_.log(id, "cached, but requires re-validation");
	} else if (new_entry->has_expiry) {
	  std::string exp_time =
	    std::asctime(std::gmtime(&new_entry->expires_at));
	  exp_time.pop_back();
	  logger_.log(id, "cached, expires at " + exp_time);
	}
	cache_.put(key, new_entry.value());
      }
    }

    return origin;
  }

  if (status == CacheStatus::NEEDS_VALIDATION && entry_opt.has_value()) {
    logger_.log(id, "in cache, requires validation");

    std::vector<std::pair<std::string,std::string>> cond;

    if (!entry_opt->etag.empty()) {
      cond.push_back({"If-None-Match", entry_opt->etag});
    }

    if (!entry_opt->last_modified.empty()) {
      cond.push_back({"If-Modified-Since", entry_opt->last_modified});
    }

    logger_.log(id,
		"Requesting \"" +
		req.method + " " +
		req.host + req.path +
		" HTTP/1.1\" from " +
		req.host);
    
    HttpResponse origin = fetcher(req, cond);

    if (origin.status_code == 0) {
      logger_.log(id, "ERROR contacting origin server");
      return origin;
    }

    logger_.log(id,
    "Received \"" +
    response_line_for_log(origin) +
    "\" from " +
    req.host);

    if (origin.status_code == 304) {
      std::cout << "[CACHE] 304 - using cached copy\n";
      std::string status_line = extract_status_line(entry_opt->raw);
      return HttpResponse{200, {}, entry_opt->raw, status_line};
    }

    std::string reason;
    auto new_entry = build_cache_entry(origin, now, reason);

    if (origin.status_code == 200) {
      if (!new_entry.has_value()) {
	logger_.log(id, "not cacheable because " + reason);
      } else {
	if (new_entry->requires_validation) {
	  logger_.log(id, "cached, but requires re-validation");
	} else if (new_entry->has_expiry) {
	  std::string exp_time =
	    std::asctime(std::gmtime(&new_entry->expires_at));
	  exp_time.pop_back();
	  logger_.log(id, "cached, expires at " + exp_time);
	}
	cache_.put(key, new_entry.value());
      }
    }

    return origin;
  }

  if (status == CacheStatus::EXPIRED) {
    if (entry_opt->has_expiry) {
      std::string exp_time =
	std::asctime(std::gmtime(&entry_opt->expires_at));
      exp_time.pop_back();
      logger_.log(id, "in cache, but expired at " + exp_time);
    } else {
      logger_.log(id, "in cache, but expired");
    }
    
    logger_.log(id,
		"Requesting \"" +
		req.method + " " +
		req.host + req.path +
		" HTTP/1.1\" from " +
		req.host);

    HttpResponse origin = fetcher(req, {});

    if (origin.status_code == 0) {
      logger_.log(id, "ERROR contacting origin server");
      return origin;
    }

    logger_.log(id,
		"Received \"" +
		response_line_for_log(origin) +
		"\" from " +
		req.host);

    std::string reason;
    auto new_entry = build_cache_entry(origin, now, reason);

    if (origin.status_code == 200) {
      if (!new_entry.has_value()) {
	logger_.log(id, "not cacheable because " + reason);
      } else {
	if (new_entry->requires_validation) {
	  logger_.log(id, "cached, but requires re-validation");
	} else if (new_entry->has_expiry) {
	  std::string exp_time =
	    std::asctime(std::gmtime(&new_entry->expires_at));
	  exp_time.pop_back();
	  logger_.log(id, "cached, expires at " + exp_time);
	}
	cache_.put(key, new_entry.value());
      }
    }

    return origin;
  }

  return fetcher(req, {});
}
