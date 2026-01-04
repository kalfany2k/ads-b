#include <rtl-sdr.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <chrono>
#include <string>
#include <cassert>

const int ADS_B_PACKET_SIZE = 112;
const int ADS_B_SAMPLE_SIZE = 224;
const int BUFFER_SIZE = 16384;
const int PREAMBLE_LENGTH = 16;
const int DOWNLINK_FORMAT_LENGTH = 5;
const uint32_t CENTER_FREQUENCY = 1090000000;
const uint32_t SAMPLE_RATE = 2000000;
const size_t MESSAGE_SIZE = 14; // Length of ADS-B payload in bytes
const uint32_t CRC_GENERATOR = 0xFFF409;

struct ADSBPacket {
    int downlink_format;
    int capability;
    std::string icao_address;
    int type;
    int altitude;
    bool time_flag;
    bool format_flag;
    float latitude;
    float longitude;
};

float compute_magnitude_mean(std::vector<float>& magnitudes, size_t size) {
    float sum = 0.0f;

    for (float magnitude : magnitudes) {
        sum += magnitude;
    }

    return sum / size;
}

float compute_magnitude_standard_deviation(std::vector<float>& magnitudes, float mean, size_t size) {
    float deviation_sum = 0.0f;

    for (float magnitude : magnitudes) {
        float diff = mean - magnitude;
        deviation_sum += diff * diff;
    }

    return std::sqrt(deviation_sum / static_cast<float>(size));
}

void set_message_bytes(const std::vector<float>& magnitudes, size_t starting_index, uint8_t message_bytes[]) {
    for (int i = 0; i < ADS_B_SAMPLE_SIZE; i += 2) {
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

void print_bytes(uint8_t bytes[]) {
    for (size_t i = 0; i < MESSAGE_SIZE; i++) {
        print_byte(bytes[i]);
    }
}

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

void extract_data(ADSBPacket& packet, const uint8_t message_bytes[]) {
    packet.downlink_format = (message_bytes[0] >> 3) & 0b11111;
    packet.capability = message_bytes[0] & 0b111;

    std::string icao_address;
    for (int i = 2; i < 5; i++) {
        icao_address.push_back(to_hex_char((message_bytes[i] >> 4) & 0xF));
        icao_address.push_back(to_hex_char(message_bytes[i] & 0xF));
    }

    packet.icao_address = icao_address;

    packet.type = message_bytes[5];

    bool special_bit = message_bytes[6] & 0b1;
    uint32_t altitude = 0 << ((message_bytes[6] >> 1) & 0b1111111) << ((message_bytes[7] >> 4) & 0b1111);
    packet.altitude = static_cast<int>(altitude) * (special_bit ? 25 : 100);

    packet.time_flag = (message_bytes[7] >> 3) & 0b1;
    packet.format_flag = (message_bytes[7] >> 2) & 0b1;

    uint32_t latitude = 0 << (message_bytes[7] & 0b11) << message_bytes[8] << ((message_bytes[9] >> 1) & 0b1111111);
    uint32_t longitude = 0 << (message_bytes[9] & 0b1) << message_bytes[10] << message_bytes[11];

    // TODO: Implement Compact Position Reporting
    packet.latitude = static_cast<float>(latitude) / 131072.0f;
    packet.longitude = static_cast<float>(longitude) / 131072.0f;
}

int is_preamble_present(const std::vector<float>& magnitudes, const size_t start_index, const float threshold) {
    return (magnitudes[start_index] > threshold) &&
        (magnitudes[start_index + 2] > threshold) &&
        (magnitudes[start_index + 7] > threshold) &&
        (magnitudes[start_index + 9] > threshold) &&
        (magnitudes[start_index + 1] < threshold * 0.5f) &&
        (magnitudes[start_index + 3] < threshold * 0.5f) &&
        (magnitudes[start_index + 8] < threshold * 0.5f);
}

int main() {
    rtlsdr_dev_t *dev = nullptr;

    if (rtlsdr_get_device_count() == 0) {
        std::cerr << "No RTL-SDR device was found.\n";
        return 1;
    }

    if (rtlsdr_open(&dev, 0) < 0) {
        std::cerr << "Failed to open device.\n";
        return 1;
    }

    rtlsdr_set_center_freq(dev, CENTER_FREQUENCY);
    rtlsdr_set_sample_rate(dev, SAMPLE_RATE);
    rtlsdr_set_tuner_gain_mode(dev, 0);

    uint8_t buffer[BUFFER_SIZE];

    std::vector<float> magnitudes;
    magnitudes.reserve(BUFFER_SIZE / 2);

    std::vector<ADSBPacket> valid_packets;

    int n_read;
    rtlsdr_reset_buffer(dev);

    while (valid_packets.size() < 100) {
        rtlsdr_read_sync(dev, buffer, sizeof(buffer), &n_read);

        // Compute all magnitudes from our received samples
        for (size_t i = 0; i < sizeof(buffer); i += 2) {
            float I = static_cast<float>(buffer[i]) - 128.0f;
            float Q = static_cast<float>(buffer[i + 1]) - 128.0f;
            magnitudes.push_back(I*I + Q*Q);
        }

        // Compute magnitude threshold to ignore frequency noise
        float magnitude_mean = compute_magnitude_mean(magnitudes, magnitudes.size());
        float magnitude_standard_deviation = compute_magnitude_standard_deviation(magnitudes, magnitude_mean, magnitudes.size());
        float threshold = magnitude_mean + 2 * magnitude_standard_deviation;

        for (size_t i = 0; i <= magnitudes.size() - ADS_B_SAMPLE_SIZE - PREAMBLE_LENGTH; i++) {
            if (magnitudes.at(i) > threshold && is_preamble_present(magnitudes, i, threshold)) {
                size_t message_start_index = i + PREAMBLE_LENGTH;
                uint8_t message_bytes[MESSAGE_SIZE] = {0};
                set_message_bytes(magnitudes, message_start_index, message_bytes);

                if (((message_bytes[0] >> 3) & 0b11111) == 17) {
                    if (is_crc_valid(message_bytes, MESSAGE_SIZE - 3)) {
                        ADSBPacket packet = {};
                        extract_data(packet, message_bytes);
                        valid_packets.push_back(packet);
                    }
                }
            }
        }

        magnitudes.clear();
    }

    rtlsdr_close(dev);
    return 0;
}