#pragma once
#include <fstream>
#include <mutex>
#include <string>

class Logger {
 public:
  Logger(const std::string& filepath);

  int next_id();
  void log(int id, const std::string& message);
  void log_noid(const std::string& message);

 private:
  std::ofstream ofs_;
  std::mutex mtx_;
  int current_id_;
};
