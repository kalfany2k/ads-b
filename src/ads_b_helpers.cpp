#include "ads_b_helpers.h"

const uint32_t CRC_GENERATOR = 0xFFF409;

bool is_crc_valid(const uint8_t message_bytes[], const size_t message_bytes_count) {
    uint32_t buffer = 0;

    for (size_t i = 0; i < message_bytes_count; i++) {
        for (size_t j = 0; j < 8; j++) {
            bool bit = (message_bytes[i] >> (7 - j)) & 0x1;
            bool msb = buffer & 0x800000;
            buffer = ((buffer << 1) | bit) & 0xFFFFFF;
            if (msb) {
                buffer ^= CRC_GENERATOR;
            }
        }
    }

    return buffer == 0;
}