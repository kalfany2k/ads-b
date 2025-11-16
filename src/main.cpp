#include <rtl-sdr.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>
#include <cassert>

int const ADS_B_PACKET_SIZE = 112;
int const BUFFER_SIZE = 16384;
int const PREAMBLE_LENGTH = 16;
std::string const PREAMBLE_SAMPLES = "1010001010000000";
int const DOWNLINK_FORMAT_LENGTH = 5;
uint32_t const CENTER_FREQUENCY = 1090000000;
uint32_t const SAMPLE_RATE = 2000000;

float compute_magnitude_mean(std::vector<float>& magnitudes, int size) {
    float sum = 0.0f;

    for (float magnitude : magnitudes) {
        sum += magnitude;
    }

    return sum / size;
}

float compute_magnitude_standard_deviation(std::vector<float>& magnitudes, float mean, int size) {
    float deviation_sum = 0.0f;

    for (float magnitude : magnitudes) {
        float diff = mean - magnitude;
        deviation_sum += diff * diff;
    }

    return std::sqrt(deviation_sum / size);
}

bool is_preamble_present(std::vector<bool>& samples) {
    std::string preamble_pulse_position_bits;
    preamble_pulse_position_bits.reserve(PREAMBLE_LENGTH);

    for (bool pulse : samples) {
        preamble_pulse_position_bits.append(pulse ? "1" : "0");
    }

    return preamble_pulse_position_bits == PREAMBLE_SAMPLES;
}

bool set_ads_b_packet_bits(std::vector<bool>& samples, std::vector<bool>& bits) {
    for (int i = 0; i < ADS_B_PACKET_SIZE * 2; i += 2) {
        if (samples.at(i) == 1 && samples.at(i + 1) == 0) {
            bits.push_back(1);
        } else if (samples.at(i) == 0 && samples.at(i + 1) == 1) {
            bits.push_back(0);
        } else {
            return false;
        }
    }

    return true;
}

std::string convert_bits_to_bitstring(std::vector<bool>& bits) {
    std::string bitstring;
    bitstring.reserve(bits.size() / 2);

    for (bool bit : bits) {
        bitstring.append(bit ? "1" : "0");
    }
    
    return bitstring;
}

void set_samples_from_magnitudes(std::vector<float>& magnitudes, std::vector<bool>& samples, float threshold) {
    for (size_t i = 0; i < magnitudes.size(); i++) {
        if (magnitudes.at(i) > threshold) {
            samples.at(i) = 1;
        }
    }
}

int main()
{
    rtlsdr_dev_t *dev = nullptr;
    int device_count = rtlsdr_get_device_count();

    if (device_count == 0)
    {
        std::cerr << "No RTL-SDR device was found.\n";
        return 1;
    }

    if (rtlsdr_open(&dev, 0) < 0)
    {
        std::cerr << "Failed to open device.\n";
        return 1;
    }

    rtlsdr_set_center_freq(dev, CENTER_FREQUENCY);
    rtlsdr_set_sample_rate(dev, SAMPLE_RATE);
    rtlsdr_set_tuner_gain_mode(dev, 0);

    uint8_t buffer[BUFFER_SIZE];
    
    std::vector<float> magnitudes; 
    magnitudes.reserve(BUFFER_SIZE / 2);

    int n_read;
    rtlsdr_reset_buffer(dev);

    while (true)
    {
        rtlsdr_read_sync(dev, buffer, sizeof(buffer), &n_read);

        // Compute all magnitudes from our received samples
        for (size_t i = 0; i < sizeof(buffer); i += 2)
        {
            float I = (float)buffer[i] - 127.5f;
            float Q = (float)buffer[i + 1] - 127.5f;
            float magnitude = std::sqrt(I*I + Q*Q);
            magnitudes.push_back(magnitude);
        }
        
        // Compute magnitude threshold to ignore frequency noise
        float magnitude_mean = compute_magnitude_mean(magnitudes, magnitudes.size());
        float magnitude_standard_deviation = compute_magnitude_standard_deviation(magnitudes, magnitude_mean, magnitudes.size());
        float threshold = magnitude_mean + 2  * magnitude_standard_deviation;

        // int magnitude_over_threshold_count = 0;

        std::vector<bool> samples;
        samples.resize(magnitudes.size(), 0);
        set_samples_from_magnitudes(magnitudes, samples, threshold);

        for (size_t i = 0; i < samples.size(); i++) {
            if (samples.at(i) == 1 && i < samples.size() - PREAMBLE_LENGTH - (ADS_B_PACKET_SIZE * 2)) {
                std::vector<bool> preamble_samples(samples.begin() + i, samples.begin() + i + PREAMBLE_LENGTH);
                
                if (is_preamble_present(preamble_samples)) {
                    std::cout << "Preamble found!\n";

                    std::vector<bool> ads_b_packet_samples(
                        samples.begin() + i + PREAMBLE_LENGTH, samples.begin() + i + PREAMBLE_LENGTH + ADS_B_PACKET_SIZE * 2
                    );
                    
                    std::vector<bool> ads_b_packet_bits;
                    ads_b_packet_bits.reserve(ADS_B_PACKET_SIZE);
                    bool were_bits_set = set_ads_b_packet_bits(ads_b_packet_samples, ads_b_packet_bits);
                    
                    std::vector<bool> ads_b_downlink_format_bits(
                        ads_b_packet_bits.begin(), ads_b_packet_bits.begin() + DOWNLINK_FORMAT_LENGTH
                    );
                    
                    if (ads_b_packet_bits.size() > 5 && convert_bits_to_bitstring(ads_b_downlink_format_bits) == "10001") {
                        std::cout << "Downlink format found!\n";
                    } else {
                        continue;
                    }
                }
            }
        }
        
        // if (magnitude_over_threshold_count >= 112) {
        //     std::cout << "Mean: " << magnitude_mean << " | Standard Deviation: " << magnitude_standard_deviation << " | Threshold: " <<     threshold << "\n" << "Samples: " << magnitudes.size() << " | Above Threshold: " << magnitude_over_threshold_count << " | Below " << magnitudes.size() - magnitude_over_threshold_count << "\n";
        // } 

        magnitudes.clear();
    }

    rtlsdr_close(dev);
    return 0;
}