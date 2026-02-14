#include "byte_manipulation.h"
#include "vector"
#include "iostream"

void set_message_bytes(const std::vector<float>& magnitudes, const size_t starting_index, uint8_t message_bytes[], const size_t sample_size) {
    for (size_t i = 0; i < sample_size; i += 2) {
        bool bit = magnitudes.at(starting_index + i) > magnitudes.at(starting_index + i + 1);
        size_t bit_number = i / 2;
        size_t byte_index = bit_number / 8;
        size_t bit_offset = 7 - (bit_number % 8);
        if (bit) {
            message_bytes[byte_index] |= (1 << bit_offset);
        }
    }
}

char to_hex_char(uint8_t value) {
    if (value < 10) {
        return '0' + value;
    } else {
        return 'A' + (value - 10);
    }
}

void print_byte(const uint8_t byte) {
    std::cout << to_hex_char((byte >> 4) & 0xF) << to_hex_char(byte & 0xF);
}

void print_bytes(const uint8_t bytes[], const size_t message_size) {
    for (size_t i = 0; i < message_size; i++) {
        print_byte(bytes[i]);
    }
}