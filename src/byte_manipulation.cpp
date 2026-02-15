#include "byte_manipulation.h"

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

uint32_t extract_bits(const uint8_t message_bytes[], const size_t from_index, const size_t to_index) {
    uint32_t buffer = 0;
    
    for (size_t i = from_index; i < to_index; i++) {
        size_t current_byte_index = i / 8;
        size_t current_bit_offset = 7 - (i % 8);
        buffer = (buffer << 1) | ((message_bytes[current_byte_index] >> current_bit_offset) & 0b1);
    }

    return buffer;
}

char to_hex_char(const uint8_t value) {
    if (value < 10) {
        return '0' + value;
    } else {
        return 'A' + (value - 10);
    }
}

void print_byte(const uint8_t byte) {
    std::cout << to_hex_char((byte >> 4) & 0xF) << to_hex_char(byte & 0xF);
}

std::string uint32_t_to_string(const uint32_t value) {
    std::string buffer;

    for (size_t i = 0; i < 6; i++) {
        buffer.push_back(to_hex_char((value >> (5 - i) * 4) & 0xF));
    }

    return buffer;
}

void print_bytes(const uint8_t bytes[], const size_t message_size) {
    for (size_t i = 0; i < message_size; i++) {
        print_byte(bytes[i]);
    }
}