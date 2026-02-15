#ifndef ADS_B_BYTE_MANIPULATION_H
#define ADS_B_BYTE_MANIPULATION_H

#include <vector>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <cassert>

void set_message_bytes(const std::vector<float>& magnitudes, const size_t starting_index, uint8_t message_bytes[], const size_t sample_size);
uint32_t extract_bits(const uint8_t message_bytes[], const size_t from_index, const size_t to_index);
char to_hex_char(const uint8_t value);
void print_byte(const uint8_t byte);
void print_bytes(const uint8_t bytes[], const size_t message_size);
std::string uint32_t_to_string(const uint32_t value);

#endif //ADS_B_BYTE_MANIPULATION_H