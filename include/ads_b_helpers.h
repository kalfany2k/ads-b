#ifndef ADS_B_ADS_B_HELPERS_H
#define ADS_B_ADS_B_HELPERS_H

#include <cstdint>
#include <cstddef>

bool is_crc_valid(const uint8_t message_bytes[], const size_t message_bytes_count);

#endif //ADS_B_ADS_B_HELPERS_H