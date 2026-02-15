#include <catch2/catch_test_macros.hpp>
#include "ads_b_helpers.h"
#include "byte_manipulation.h"

// 8D4840D6202CC371C32CE0576098

TEST_CASE("CRC is valid for known ADS-B packet") {
    uint8_t valid_msg[] = {
        0x8D, 0x48, 0x40, 0xD6, 0x20, 0x2C, 0xC3, 0x71, 0xC3, 0x2C, 0xE0, 0x57, 0x60, 0x98
    };

    REQUIRE(is_crc_valid(valid_msg, 14));
}

TEST_CASE("CRC fails for corrupted packet") {
    uint8_t corrupt_msg[] = {
        0x8D, 0x48, 0x40, 0xD6, 0x20, 0x2C, 0xC3, 0x71, 0xC3, 0x2C, 0xE0, 0x57, 0x60, 0x99
    };

    REQUIRE_FALSE(is_crc_valid(corrupt_msg, 14));
}

TEST_CASE("extract_bits works correctly") {
    struct TestCase {
        uint8_t bytes[14];
        size_t from_index;
        size_t to_index;
        uint32_t expected;
    };

    TestCase cases[] = {
        {{0b11111000}, 0, 5, 0b11111},
        {{0b00000111}, 5, 8, 0b111},
        {{0xFF, 0x00}, 0, 8, 0xFF},
    };

    for (auto& tc : cases) {
        REQUIRE(extract_bits(tc.bytes, tc.from_index, tc.to_index) == tc.expected);
    }
}