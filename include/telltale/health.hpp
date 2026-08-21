#pragma once

#include <string>

namespace telltale {

struct HealthReport {
  std::string status;  // "ok"
  std::string version;
  std::string build;
  bool logging_ready;
  bool suite_entrypoints_present;
  std::string logging_framework;
  std::string error_tracking;
  std::string metrics_module;
};

// Machine-readable health snapshot for ops/automation (not a network server).
HealthReport build_health_report();
std::string format_health_report_json(const HealthReport& report);

}  // namespace telltale
