#pragma once
#include <cstdint>
#include <vector>

namespace TestData {

// Deterministic pseudo-random binary data (xorshift32).
inline std::vector<uint8_t> random_binary(size_t bytes, uint32_t seed = 42) {
    std::vector<uint8_t> data(bytes);
    uint32_t s = seed ? seed : 1u;
    for (auto& b : data) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        b = static_cast<uint8_t>(s & 0xFFu);
    }
    return data;
}

// Highly compressible: repeating 32-byte cycle.
inline std::vector<uint8_t> compressible(size_t bytes) {
    std::vector<uint8_t> data(bytes);
    for (size_t i = 0; i < bytes; ++i)
        data[i] = static_cast<uint8_t>(i % 32u);
    return data;
}

// Low-entropy alias (different seed from random_binary default).
inline std::vector<uint8_t> incompressible(size_t bytes) {
    return random_binary(bytes, 12345u);
}

// Printable ASCII with word-like patterns.
inline std::vector<uint8_t> text_like(size_t bytes) {
    static const char* words[] = {
        "the ", "quick ", "brown ", "fox ",
        "jumps ", "over ", "the ", "lazy ", "dog "
    };
    constexpr size_t nwords = 9;
    std::vector<uint8_t> data;
    data.reserve(bytes);
    size_t wi = 0;
    while (data.size() < bytes) {
        const char* w = words[wi % nwords];
        for (size_t i = 0; w[i] && data.size() < bytes; ++i)
            data.push_back(static_cast<uint8_t>(w[i]));
        ++wi;
    }
    return data;
}

} // namespace TestData
