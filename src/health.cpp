#include "telltale/health.hpp"

#include <sstream>

#include "telltale/error_tracking.hpp"
#include "telltale/logging.hpp"
#include "telltale/metrics.hpp"

namespace telltale {

HealthReport build_health_report() {
  HealthReport r;
  r.status = "ok";
  r.version = "0.2.0";
  r.build = "release";
  r.logging_ready = true;
  r.suite_entrypoints_present = true;
  r.logging_framework = LOGGING_FRAMEWORK;
  r.error_tracking = ERROR_TRACKING;
  r.metrics_module = METRICS_MODULE;
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
      << ","
      << "\"logging_framework\":\"" << report.logging_framework << "\","
      << "\"error_tracking\":\"" << report.error_tracking << "\","
      << "\"metrics\":\"" << report.metrics_module << "\""
      << "}";
  return oss.str();
}

}  // namespace telltale
