#pragma once
#include "../http/http_types.hpp"
#include <ctime>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct CacheEntry {
  std::vector<uint8_t> raw;     
  std::time_t stored_at = 0;

  bool has_expiry = false;
  std::time_t expires_at = 0;

  bool requires_validation = false;
  std::string etag;
  std::string last_modified;
};

class Cache {
 public:
  explicit Cache(size_t max_entries = 1024);

  std::optional<CacheEntry> get(const std::string &key) const;
  void put(const std::string &key, const CacheEntry &entry);
  void erase(const std::string &key);

  size_t size() const;

 private:
  void evict_if_needed_locked();

  size_t max_entries_;
  mutable std::mutex mtx_;
  std::unordered_map<std::string, CacheEntry> map_;

  std::deque<std::string> fifo_;
};

enum class CacheStatus {
  MISS,
  EXPIRED,
  NEEDS_VALIDATION,
  FRESH
};

CacheStatus classify(const std::optional<CacheEntry> &entry,
                     std::time_t now);


std::optional<CacheEntry> build_cache_entry(
    const HttpResponse &resp,
    std::time_t now,
    std::string &reason);
