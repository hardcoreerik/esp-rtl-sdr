#pragma once

#include <cstdint>

struct RtlControlRecord {
    uint16_t value;
    uint16_t index;
    uint8_t request_type;
    uint8_t length;
    uint8_t data[8];
};
