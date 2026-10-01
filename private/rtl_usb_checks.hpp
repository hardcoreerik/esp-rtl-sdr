#pragma once

/**
 * Host-testable USB sanity checks: control-transfer bounds and the descriptor
 * layout this driver expects. No USB host stack, FreeRTOS or ESP-IDF; the
 * driver calls these with the raw descriptor bytes and transfer sizes.
 */

#include <cstddef>
#include <cstdint>

/** bmRequestType direction bit (device-to-host). */
static constexpr uint8_t kRtlUsbDirIn = 0x80;
/** Size of the setup packet that precedes the data stage in a control buffer. */
static constexpr size_t kRtlUsbSetupBytes = 8;
/** RTL2832U I2C passthrough (block IICB) reads longer than this STALL EP0. */
static constexpr uint16_t kRtlI2cMaxReadBytes = 16;

/**
 * Whether a control request fits the transfer buffer and the RTL2832U accepts it.
 * buffer_bytes is the whole control transfer buffer, setup packet included.
 */
inline bool rtl_ctrl_request_ok(uint8_t bm_request_type, uint16_t w_index, uint16_t w_length,
                                size_t buffer_bytes)
{
    if (buffer_bytes < kRtlUsbSetupBytes ||
        static_cast<size_t>(w_length) > buffer_bytes - kRtlUsbSetupBytes) {
        return false;
    }
    /* Block 6 (IICB) read: wIndex = 0x0600. Writes to the same block use 0x0610. */
    const bool i2c_read = (bm_request_type & kRtlUsbDirIn) != 0 && (w_index & 0xff00u) == 0x0600u;
    return !(i2c_read && w_length > kRtlI2cMaxReadBytes);
}

/**
 * Data-stage bytes actually received, from a control transfer's actual_num_bytes
 * (which counts the setup packet too).
 */
inline uint16_t rtl_ctrl_data_stage_bytes(int actual_num_bytes)
{
    if (actual_num_bytes <= static_cast<int>(kRtlUsbSetupBytes)) {
        return 0;
    }
    const int data = actual_num_bytes - static_cast<int>(kRtlUsbSetupBytes);
    return data > 0xffff ? static_cast<uint16_t>(0xffff) : static_cast<uint16_t>(data);
}

/**
 * Whether a configuration descriptor (raw bytes, as returned by the host stack)
 * has interface 0, alternate setting 0, with a bulk IN endpoint ep_addr whose max
 * packet size is between 8 and 512 bytes. Malformed descriptors are rejected.
 */
inline bool rtl_config_desc_has_bulk_in(const uint8_t *cfg, size_t len, uint8_t ep_addr)
{
    if (cfg == nullptr || len < 9 || cfg[0] < 9 || cfg[1] != 0x02) {
        return false;
    }
    size_t total = static_cast<size_t>(cfg[2]) | (static_cast<size_t>(cfg[3]) << 8);
    if (total > len) {
        total = len;
    }
    bool in_intf0 = false;
    size_t off = 0;
    while (off + 2 <= total) {
        const uint8_t b_length = cfg[off];
        const uint8_t b_type = cfg[off + 1];
        if (b_length < 2 || off + b_length > total) {
            return false;
        }
        const uint8_t *d = cfg + off;
        if (b_type == 0x04 && b_length >= 9) {
            in_intf0 = d[2] == 0 && d[3] == 0;
        } else if (b_type == 0x05 && b_length >= 7 && in_intf0 && d[2] == ep_addr) {
            if ((d[3] & 0x03) != 0x02) {
                return false;
            }
            const uint16_t mps =
                static_cast<uint16_t>((d[4] | (d[5] << 8)) & 0x07ff);
            return mps >= 8 && mps <= 512;
        }
        off += b_length;
    }
    return false;
}
