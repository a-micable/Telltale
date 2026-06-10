#pragma once

#include "telltale/types.hpp"
#include "telltale/errors.hpp"
#include <vector>
#include <cstdint>

namespace telltale {

class SchemaUpdateCodec {
public:
    static constexpr size_t MIN_PAYLOAD_SIZE = 3;
    static constexpr size_t FULL_PAYLOAD_SIZE = 5;
    static constexpr size_t RESIZE_PAYLOAD_SIZE = 7;

    static std::vector<uint8_t> encode(uint8_t flags, uint16_t type_id,
                                        uint16_t handler_id, bool include_handler_id);
    static Result decode(const std::vector<uint8_t>& payload, SchemaUpdatePayload& out);
    static Result decode(const uint8_t* payload, size_t payload_len, SchemaUpdatePayload& out);

    static bool requires_handler_id(uint8_t flags);
    static bool requires_table_size(uint8_t flags);
    static bool is_valid_flags(uint8_t flags);
    static std::string flags_to_string(uint8_t flags);

    static Result validate_payload(const std::vector<uint8_t>& payload);
    static size_t expected_payload_size(uint8_t flags);
};

}  // namespace telltale
