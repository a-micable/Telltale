#include "telltale/health.hpp"

#include <sstream>

#include "telltale/logging.hpp"

namespace telltale {

HealthReport build_health_report() {
  HealthReport r;
  r.status = "ok";
  r.version = "0.2.0";
  r.build = "release";
  r.logging_ready = true;
  r.suite_entrypoints_present = true;
  log().info("health", "health report generated");
  return r;
}

std::string format_health_report_json(const HealthReport& report) {
  std::ostringstream oss;
  oss << "{"
      << "\"status\":\"" << report.status << "\","
      << "\"version\":\"" << report.version << "\","
      << "\"build\":\"" << report.build << "\","
      << "\"logging_ready\":" << (report.logging_ready ? "true" : "false") << ","
      << "\"suite_entrypoints_present\":" << (report.suite_entrypoints_present ? "true" : "false")
      << "}";
  return oss.str();
}

}  // namespace telltale
