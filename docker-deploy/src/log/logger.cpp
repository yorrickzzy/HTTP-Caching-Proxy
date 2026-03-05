#include "logger.hpp"
#include <ctime>

Logger::Logger(const std::string& filepath)
    : ofs_(filepath, std::ios::app), current_id_(0) {}

int Logger::next_id() {
  std::lock_guard<std::mutex> lock(mtx_);
  return ++current_id_;
}

void Logger::log(int id, const std::string& message) {
  std::lock_guard<std::mutex> lock(mtx_);

  std::time_t now = std::time(nullptr);
  std::string time_str = std::asctime(std::gmtime(&now));
  time_str.pop_back();  

  ofs_ << id << ": " << message << "\n";
  ofs_.flush();
}

void Logger::log_noid(const std::string& message) {
  std::lock_guard<std::mutex> lock(mtx_);
  ofs_ << "(no-id): " << message << "\n";
  ofs_.flush();
}
