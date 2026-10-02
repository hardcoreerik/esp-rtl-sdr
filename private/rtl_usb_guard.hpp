#pragma once

/**
 * USB safety rules for the RTL2832U driver, kept free of the USB host stack, FreeRTOS and ESP-IDF so the
 * host tests can exercise them directly. Written from this project's own requirements and measurements:
 * what a control transfer may ask for, what a usable configuration descriptor looks like, and when to
 * power-cycle the root port after a failed enumeration.
 */

#include <cstddef>
#include <cstdint>

/* ----------------------------------------------------------------------------------------------- */
/* Control transfers                                                                                */
/* ----------------------------------------------------------------------------------------------- */

namespace rtl_usb {

constexpr size_t kSetupBytes = 8;       /**< USB setup packet that precedes the data stage. */
constexpr uint8_t kDirDeviceToHost = 0x80;
/** The RTL2832U routes I2C passthrough through "block 6" (high byte of wIndex); reads there are capped. */
constexpr uint8_t kI2cBlock = 0x06;
constexpr uint16_t kI2cReadCapBytes = 16;

constexpr bool is_in(uint8_t bm_request_type) { return (bm_request_type & kDirDeviceToHost) != 0; }
constexpr bool is_i2c_block(uint16_t w_index) { return static_cast<uint8_t>(w_index >> 8) == kI2cBlock; }

/**
 * May this request be sent? buffer_bytes is the whole control buffer (setup packet included). A request is
 * refused when its data stage would not fit, or when it reads more than the I2C passthrough can return.
 */
constexpr bool ctrl_request_allowed(uint8_t bm_request_type, uint16_t w_index, uint16_t w_length,
                                    size_t buffer_bytes)
{
    if (buffer_bytes < kSetupBytes) return false;
    if (static_cast<size_t>(w_length) > buffer_bytes - kSetupBytes) return false;
    if (is_in(bm_request_type) && is_i2c_block(w_index) && w_length > kI2cReadCapBytes) return false;
    return true;
}

/** Data-stage bytes of a finished control transfer, from the host stack's total (which includes the setup packet). */
constexpr uint16_t ctrl_data_bytes(int total_transferred)
{
    if (total_transferred <= static_cast<int>(kSetupBytes)) return 0;
    const int data = total_transferred - static_cast<int>(kSetupBytes);
    return data > 0xffff ? static_cast<uint16_t>(0xffff) : static_cast<uint16_t>(data);
}

/* ----------------------------------------------------------------------------------------------- */
/* Configuration descriptor                                                                         */
/* ----------------------------------------------------------------------------------------------- */

namespace desc {
constexpr uint8_t kTypeConfig = 0x02;
constexpr uint8_t kTypeInterface = 0x04;
constexpr uint8_t kTypeEndpoint = 0x05;
constexpr uint16_t kBulkMinPacket = 8;
constexpr uint16_t kBulkMaxPacket = 512;
}  // namespace desc

/**
 * Does this raw configuration descriptor expose, on interface 0 / alternate setting 0, a bulk IN endpoint at
 * ep_addr with a sane packet size (8..512)? Anything malformed fails closed.
 */
inline bool config_has_bulk_in(const uint8_t *cfg, size_t cfg_len, uint8_t ep_addr)
{
    if (cfg == nullptr || cfg_len < 9 || cfg[0] < 9 || cfg[1] != desc::kTypeConfig) return false;
    const size_t declared = static_cast<size_t>(cfg[2]) | (static_cast<size_t>(cfg[3]) << 8);
    const size_t limit = declared < cfg_len ? declared : cfg_len;

    bool in_first_alt = false;
    for (size_t pos = 0; pos + 2 <= limit;) {
        const uint8_t length = cfg[pos];
        const uint8_t type = cfg[pos + 1];
        if (length < 2 || pos + length > limit) return false;
        const uint8_t *d = cfg + pos;
        if (type == desc::kTypeInterface && length >= 9) {
            in_first_alt = d[2] == 0 && d[3] == 0;  /* bInterfaceNumber, bAlternateSetting */
        } else if (type == desc::kTypeEndpoint && length >= 7 && in_first_alt && d[2] == ep_addr) {
            const bool is_bulk = (d[3] & 0x03) == 0x02;
            const uint16_t packet = static_cast<uint16_t>((d[4] | (d[5] << 8)) & 0x07ff);
            return is_bulk && packet >= desc::kBulkMinPacket && packet <= desc::kBulkMaxPacket;
        }
        pos += length;
    }
    return false;
}

/* ----------------------------------------------------------------------------------------------- */
/* Failed-enumeration recovery                                                                      */
/* ----------------------------------------------------------------------------------------------- */

/**
 * When a dongle fails to enumerate (seen on ESP32-P4 resets with the dongle attached) nothing retries until it is
 * replugged. This tracks how long the bus has had no enumerated device and says when to power-cycle the root
 * port, waiting 10 s, then 20, 40 and 60 s between cycles so an empty port is only blipped about once a minute.
 * Pure timing logic: the caller supplies a millisecond clock (wrap-safe) and does the power cycle.
 */
class EnumRetry {
public:
    static constexpr uint32_t kFirstWaitMs = 10000;
    static constexpr uint32_t kMaxWaitMs = 60000;

    /** A device is enumerated or open: forget any pending retry and start over at the first wait. */
    void device_present()
    {
        counting_ = false;
        wait_ms_ = kFirstWaitMs;
    }

    /** The bus has no enumerated device at now_ms. Returns true when the root port should be power-cycled now. */
    bool no_device(uint32_t now_ms)
    {
        if (!counting_) {
            counting_ = true;
            since_ms_ = now_ms;
            return false;
        }
        if (static_cast<uint32_t>(now_ms - since_ms_) < wait_ms_) return false;
        since_ms_ = now_ms;
        wait_ms_ = (wait_ms_ * 2 > kMaxWaitMs) ? kMaxWaitMs : wait_ms_ * 2;
        return true;
    }

    /** After a power cycle the device needs time to come back: restart the wait from now. */
    void cycled(uint32_t now_ms) { since_ms_ = now_ms; }

    uint32_t current_wait_ms() const { return wait_ms_; }
    bool counting() const { return counting_; }

private:
    bool counting_ = false;
    uint32_t since_ms_ = 0;
    uint32_t wait_ms_ = kFirstWaitMs;
};

}  // namespace rtl_usb
