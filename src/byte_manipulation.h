#ifndef ADS_B_BYTE_MANIPULATION_H
#define ADS_B_BYTE_MANIPULATION_H

#include "vector"

void set_message_bytes(const std::vector<float>& magnitudes, size_t starting_index, uint8_t message_bytes[], size_t sample_size);
char to_hex_char(uint8_t value);
void print_byte(uint8_t byte);
void print_bytes(uint8_t bytes[], size_t message_size);

#endif //ADS_B_BYTE_MANIPULATION_H