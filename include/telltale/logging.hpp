#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace telltale {

enum class LogLevel { Info = 0, Warn = 1, Error = 2 };

// Minimal leveled logger with timestamp and module tag.
class Logger {
 public:
  static Logger& instance() {
    static Logger logger;
    return logger;
  }

  void set_level(LogLevel level) { level_ = level; }
  LogLevel level() const { return level_; }

  void set_stream(std::ostream* out) { out_ = out ? out : &std::cerr; }
  std::ostream& stream() { return *out_; }

  void log(LogLevel level, const std::string& module, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(level_)) return;
    std::lock_guard<std::mutex> lock(mu_);
    *out_ << format(level, module, message) << std::endl;
  }

  void info(const std::string& module, const std::string& message) {
    log(LogLevel::Info, module, message);
  }
  void warn(const std::string& module, const std::string& message) {
    log(LogLevel::Warn, module, message);
  }
  void error(const std::string& module, const std::string& message) {
    log(LogLevel::Error, module, message);
  }

  static const char* level_name(LogLevel level) {
    switch (level) {
      case LogLevel::Info:
        return "INFO";
      case LogLevel::Warn:
        return "WARN";
      case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
  }

  static std::string format(LogLevel level, const std::string& module, const std::string& message) {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t t = clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ") << " [" << level_name(level) << "] [" << module
        << "] " << message;
    return oss.str();
  }

 private:
  Logger() : level_(LogLevel::Info), out_(&std::cerr) {}

  LogLevel level_;
  std::ostream* out_;
  std::mutex mu_;
};

inline Logger& log() { return Logger::instance(); }

}  // namespace telltale
