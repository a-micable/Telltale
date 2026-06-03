#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace telltale {

class Crc32 {
public:
    static constexpr uint32_t INITIAL = 0xFFFFFFFFU;
    static constexpr uint32_t FINAL_XOR = 0xFFFFFFFFU;

    Crc32();
    void reset();
    void update(const uint8_t* data, size_t length);
    void update(uint8_t byte);
    uint32_t finalize() const;
    uint32_t compute(const uint8_t* data, size_t length) const;

    static uint32_t of_bytes(const uint8_t* data, size_t length);
    static uint32_t of_vector(const std::vector<uint8_t>& data);
    static uint32_t of_record(uint16_t type_id, const std::vector<uint8_t>& payload);
    static uint32_t of_header_fields(uint16_t version, uint32_t record_count);

private:
    uint32_t state_;
    static const uint32_t TABLE[256];
    static void init_table(uint32_t table[256]);
};

}  // namespace telltale
