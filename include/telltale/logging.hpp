#pragma once

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace telltale {

// Named logging_framework for docs and static discovery (structured key=value logger).
inline constexpr const char* LOGGING_FRAMEWORK = "telltale_structured_logger";

enum class LogLevel { Info = 0, Warn = 1, Error = 2 };

// Structured logger: emits key=value fields (ts, level, module, msg).
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

  // Reads TELLTALE_LOG_LEVEL from the process environment (info|warn|error).
  void configure_from_env() {
    const char* raw = std::getenv("TELLTALE_LOG_LEVEL");
    if (!raw || !*raw) return;
    std::string v(raw);
    for (char& c : v) {
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    if (v == "warn" || v == "warning") {
      set_level(LogLevel::Warn);
    } else if (v == "error" || v == "err") {
      set_level(LogLevel::Error);
    } else {
      set_level(LogLevel::Info);
    }
  }

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

  // Emit a raw line on the configured stream (for machine-readable CLI output).
  void emit_raw(const std::string& line) {
    std::lock_guard<std::mutex> lock(mu_);
    *out_ << line << std::endl;
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

  // Example: ts=2026-08-21T19:00:00Z level=WARN module=network msg=hello
  static std::string format(LogLevel level, const std::string& module, const std::string& message) {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t t = clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << "ts=" << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ") << " level=" << level_name(level)
        << " module=" << module << " msg=" << message;
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
