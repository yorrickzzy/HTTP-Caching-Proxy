#include "cache.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

static std::optional<int> parse_max_age(const std::string &cc) {
  std::string s = cc;
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  auto pos = s.find("max-age=");
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  pos += 8;
  if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) {
    return std::nullopt;
  }
  int value = 0;
  while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
    value = value * 10 + (s[pos] - '0');
    pos++;
  }
  return value;
}

struct CacheControlFlags {
  bool no_store = false;
  bool requires_validation = false;
};

static CacheControlFlags parse_cache_control_flags(const std::string &cc) {
  CacheControlFlags flags;
  std::string s = cc;
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  if (s.find("no-store") != std::string::npos) {
    flags.no_store = true;
  }
  if (s.find("no-cache") != std::string::npos ||
      s.find("must-revalidate") != std::string::npos) {
    flags.requires_validation = true;
  }
  return flags;
}

static std::optional<std::time_t> parse_http_date(const std::string &date_str) {
  std::tm tm = {};
  std::istringstream ss(date_str);
  ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
  if (ss.fail()) {
    return std::nullopt;
  }
  return timegm(&tm);
}

std::optional<CacheEntry> build_cache_entry(
    const HttpResponse &resp,
    std::time_t now,
    std::string &reason) {
  if (resp.status_code != 200) {
    reason = "response status is not 200";
    return std::nullopt;
  }
  CacheEntry entry;
  entry.raw = resp.raw;
  entry.stored_at = now;
  auto it = resp.headers.find("Cache-Control");
  if (it != resp.headers.end()) {
    auto flags = parse_cache_control_flags(it->second);

    if (flags.no_store) {
      reason = "Cache-Control: no-store";
      return std::nullopt;
    }
    if (flags.requires_validation) {
      entry.requires_validation = true;
    }
    auto max_age = parse_max_age(it->second);
    if (max_age.has_value()) {
      entry.has_expiry = true;
      entry.expires_at = now + max_age.value();
    }
  }
  if (!entry.has_expiry) {
    auto exp = resp.headers.find("Expires");
    if (exp != resp.headers.end()) {
      auto parsed = parse_http_date(exp->second);
      if (parsed.has_value()) {
        entry.has_expiry = true;
        entry.expires_at = parsed.value();
      }
    }
  }
  if (!entry.has_expiry && !entry.requires_validation) {
    entry.requires_validation = true;
  }
  auto et = resp.headers.find("ETag");
  if (et != resp.headers.end()) {
    entry.etag = et->second;
  }
  auto lm = resp.headers.find("Last-Modified");
  if (lm != resp.headers.end()) {
    entry.last_modified = lm->second;
  }
  return entry;
}

Cache::Cache(size_t max_entries) : max_entries_(max_entries) {}

std::optional<CacheEntry> Cache::get(const std::string &key) const {
  std::lock_guard<std::mutex> lk(mtx_);
  auto it = map_.find(key);
  if (it == map_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void Cache::put(const std::string &key, const CacheEntry &entry) {
  std::lock_guard<std::mutex> lk(mtx_);
  bool existed = (map_.find(key) != map_.end());
  map_[key] = entry;
  if (!existed) {
    fifo_.push_back(key);
  }

  evict_if_needed_locked();
}

void Cache::erase(const std::string &key) {
  std::lock_guard<std::mutex> lk(mtx_);
  map_.erase(key);
}

size_t Cache::size() const {
  std::lock_guard<std::mutex> lk(mtx_);
  return map_.size();
}

void Cache::evict_if_needed_locked() {
  while (map_.size() > max_entries_) {
    while (!fifo_.empty()) {
      std::string victim = fifo_.front();
      fifo_.pop_front();
      auto it = map_.find(victim);
      if (it != map_.end()) {
        map_.erase(it);
        break;
      }
    }
    if (fifo_.empty()) {
      break;
    }
  }
}

CacheStatus classify(const std::optional<CacheEntry> &entry,
                     std::time_t now) {
  if (!entry.has_value()) {
    return CacheStatus::MISS;
  }
  const CacheEntry &e = entry.value();
  if (e.requires_validation) {
    return CacheStatus::NEEDS_VALIDATION;
  }
  if (e.has_expiry && now > e.expires_at) {
    return CacheStatus::EXPIRED;
  }
  return CacheStatus::FRESH;
}
