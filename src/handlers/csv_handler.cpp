#include "telltale/handlers/csv_handler.hpp"
#include <cstring>
#include <algorithm>

namespace telltale::handlers {

Csv_Handler::Csv_Handler() {}

Csv_Handler::~Csv_Handler() {
    buffer_.clear();
}

void Csv_Handler::initialize(const Config& cfg) {
    config_ = cfg;
    buffer_.reserve(cfg.buffer_size);
}

bool Csv_Handler::handle_event(const uint8_t* data, size_t size) {
    if (!data || size == 0) return false;
    
    if (config_.validate && !validate_event(data, size)) {
        return false;
    }
    
    if (!process_payload(data, size)) {
        return false;
    }
    
    processed_++;
    return true;
}

std::vector<uint8_t> Csv_Handler::get_result() const {
    return buffer_;
}

bool Csv_Handler::validate_event(const uint8_t* data, size_t size) {
    if (size < 4) return false;
    uint32_t len = *reinterpret_cast<const uint32_t*>(data);
    return len == size;
}

bool Csv_Handler::process_payload(const uint8_t* data, size_t size) {
    try {
        buffer_.insert(buffer_.end(), data, data + size);
        if (buffer_.size() > config_.buffer_size) {
            buffer_.erase(buffer_.begin(), 
                         buffer_.begin() + (buffer_.size() - config_.buffer_size));
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace telltale::handlers
