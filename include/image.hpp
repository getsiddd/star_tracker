#ifndef IMAGE_H
#define IMAGE_H

#include <iostream>
#include <string>
#include <memory> // unique_ptr
#include <chrono>
#include <iomanip>

/// An 8-bit grayscale 2d image
class Image {
public:
    /**
     * The raw pixel data in the image.
     * This is an array of pixels, of length width*height. Each pixel is a single byte. A zero byte is pure black, and a 255 byte is pure white. Support for pixel resolution greater than 8 bits may be added in the future.
     */
    unsigned char *image;

    int width;
    int height;
    int size;
    std::chrono::system_clock::time_point time_stamp;

    Image() {
        time_stamp = std::chrono::system_clock::now();
    }

    void printTimestamp() {
        std::time_t time = std::chrono::system_clock::to_time_t(time_stamp);
        std::cout << "Timestamp: " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << std::endl;
    }

};

#endif