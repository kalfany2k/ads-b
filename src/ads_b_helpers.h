#ifndef ADS_B_ADS_B_HELPERS_H
#define ADS_B_ADS_B_HELPERS_H

#include "vector"

bool is_crc_valid(uint8_t message_bytes[], size_t message_bytes_count);

#endif //ADS_B_ADS_B_HELPERS_H