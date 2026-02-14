#include <rtl-sdr.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <string>
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

void print_bytes(uint8_t bytes[]) {
    for (size_t i = 0; i < MESSAGE_SIZE; i++) {
        print_byte(bytes[i]);
    }
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
                set_message_bytes(magnitudes, message_start_index, message_bytes, ADS_B_SAMPLE_SIZE);

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