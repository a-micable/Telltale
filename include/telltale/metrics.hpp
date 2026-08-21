#pragma once

#include <atomic>
#include <cstdint>

namespace telltale {

// Lightweight process metrics (CLI/library scope — not infra/Prometheus).
inline constexpr const char* METRICS_MODULE = "telltale_metrics";

class Metrics {
 public:
  static Metrics& instance() {
    static Metrics m;
    return m;
  }

  void inc_commands() { commands_.fetch_add(1, std::memory_order_relaxed); }
  void inc_validation_failures() { validation_failures_.fetch_add(1, std::memory_order_relaxed); }
  void inc_records_written(uint64_t n) { records_written_.fetch_add(n, std::memory_order_relaxed); }

  uint64_t commands() const { return commands_.load(std::memory_order_relaxed); }
  uint64_t validation_failures() const {
    return validation_failures_.load(std::memory_order_relaxed);
  }
  uint64_t records_written() const { return records_written_.load(std::memory_order_relaxed); }

 private:
  std::atomic<uint64_t> commands_{0};
  std::atomic<uint64_t> validation_failures_{0};
  std::atomic<uint64_t> records_written_{0};
};

inline Metrics& metrics() { return Metrics::instance(); }

}  // namespace telltale
