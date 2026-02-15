#include <rtl-sdr.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <ostream>
#include <cmath>
#include <cassert>
#include "byte_manipulation.h"
#include "ads_b_helpers.h"

const size_t ADS_B_PACKET_SIZE = 112;
const size_t ADS_B_SAMPLE_SIZE = 224;
const int BUFFER_SIZE = 16384;
const int PREAMBLE_LENGTH = 16;
const int DOWNLINK_FORMAT_LENGTH = 5;
const uint32_t CENTER_FREQUENCY = 1090000000;
const uint32_t SAMPLE_RATE = 2000000;
const size_t MESSAGE_SIZE = 14; // Length of ADS-B payload in bytes

struct ADSBPacket {
    float latitude = 0.0f;
    float longitude = 0.0f;
    uint32_t icao_address = 0;
    uint16_t altitude = 0;
    uint8_t downlink_format = 0;
    uint8_t capability = 0;
    uint8_t type = 0;
    bool time_flag = false;
    bool format_flag = false;

    ADSBPacket() = default;
    
    ADSBPacket(const uint8_t message_bytes[]) {
        downlink_format = extract_bits(message_bytes, 0, 5);
        capability = extract_bits(message_bytes, 5, 8);
        icao_address = extract_bits(message_bytes, 8, 32);
        type = extract_bits(message_bytes, 32, 37);
        altitude = extract_bits(message_bytes, 40, 52);
        time_flag = extract_bits(message_bytes, 52, 53);
        format_flag = extract_bits(message_bytes, 53, 54);
        latitude = static_cast<float>(extract_bits(message_bytes, 54, 71)) / 131072.0f;
        longitude = static_cast<float>(extract_bits(message_bytes, 71, 88)) / 131072.0f;
    }
};

std::ostream& operator<<(std::ostream& os, const ADSBPacket& packet) {
    os << "ADSBPacket {\n"
       << "  downlink_format: " << static_cast<int>(packet.downlink_format) << "\n"
       << "  capability: " << static_cast<int>(packet.capability) << "\n"
       << "  icao_address: " << uint32_t_to_string(packet.icao_address) << "\n"
       << "  type: " << static_cast<int>(packet.type >> 3) << "\n"
       << "  altitude: " << packet.altitude << "\n"
       << "  time_flag: " << packet.time_flag << "\n"
       << "  format_flag: " << packet.format_flag << "\n"
       << "  latitude: " << packet.latitude << "\n"
       << "  longitude: " << packet.longitude << "\n"
       << "}";
    return os;
}

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
                set_message_bytes(magnitudes, message_start_index, message_bytes, ADS_B_SAMPLE_SIZE);

                if (((message_bytes[0] >> 3) & 0b11111) == 17) {
                    if (is_crc_valid(message_bytes, MESSAGE_SIZE)) {
                        ADSBPacket packet(message_bytes);
                        std::cout << packet << "\n";
                    }
                }
            }
        }

        magnitudes.clear();
    }

    rtlsdr_close(dev);
    return 0;
}