#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace telltale::handlers {

/**
 * @class Compression_Handler
 * Specialized event handler for compression handler
 */
class Compression_Handler {
 public:
  Compression_Handler();
  ~Compression_Handler();

  struct Config {
    bool enabled = true;
    uint32_t buffer_size = 65536;
    bool validate = true;
    bool compress = false;
    bool cache = true;
  };

  void initialize(const Config& cfg);
  bool handle_event(const uint8_t* data, size_t size);
  std::vector<uint8_t> get_result() const;
  size_t get_processed_count() const { return processed_; }

 private:
  Config config_;
  std::vector<uint8_t> buffer_;
  size_t processed_ = 0;

  bool validate_event(const uint8_t* data, size_t size);
  bool process_payload(const uint8_t* data, size_t size);
};

}  // namespace telltale::handlers
