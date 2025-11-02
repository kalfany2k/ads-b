#include <rtl-sdr.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>

int BUFFER_SIZE = 16384;

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

    rtlsdr_set_center_freq(dev, 1090000000);
    rtlsdr_set_sample_rate(dev, 2000000);
    rtlsdr_set_tuner_gain_mode(dev, 0);

    uint8_t buffer[BUFFER_SIZE];
    
    std::vector<float> magnitudes; 
    magnitudes.reserve(BUFFER_SIZE / 2);

    int n_read;
    rtlsdr_reset_buffer(dev);

    while (true)
    {
        rtlsdr_read_sync(dev, buffer, sizeof(buffer), &n_read);

        for (size_t i = 0; i < sizeof(buffer); i += 2)
        {
            float I = (float)buffer[i] - 127.5f;
            float Q = (float)buffer[i + 1] - 127.5f;
            float magnitude = std::sqrt(I*I + Q*Q);
            magnitudes.push_back(magnitude);
        }

        float magnitude_mean = compute_magnitude_mean(magnitudes, magnitudes.size());
        float magnitude_standard_deviation = compute_magnitude_standard_deviation(magnitudes, magnitude_mean, magnitudes.size());
        float threshold = magnitude_mean + 4 * magnitude_standard_deviation;

        int magnitude_over_threshold_count = 0;

        for (float magnitude : magnitudes) {
            if (magnitude > threshold) {
                magnitude_over_threshold_count++;
            }
        }
        
        if (magnitude_over_threshold_count >= 112) {
            std::cout << "Mean: " << magnitude_mean << " | Standard Deviation: " << magnitude_standard_deviation << " | Threshold: " <<     threshold << "\n" << "Samples: " << magnitudes.size() << " | Above Threshold: " << magnitude_over_threshold_count << " | Below " << magnitudes.size() - magnitude_over_threshold_count << "\n";
        } 

        magnitudes.clear();
    }

    rtlsdr_close(dev);
    return 0;
}