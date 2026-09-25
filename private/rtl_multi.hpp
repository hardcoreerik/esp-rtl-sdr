#pragma once

/**
 * Host-testable multi-receiver primitives: exclusive USB-address claims,
 * logical index allocation, sequence numbers, identity/path formatting.
 * No USB, FreeRTOS, or ESP-IDF. The streaming driver uses these so two
 * handles cannot silently share a dongle.
 */

#include "esp_rtl_sdr.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

struct RtlClaimTable {
    uint8_t addr[ESP_RTL_SDR_MAX_DEVICES]{};
    uint8_t owner[ESP_RTL_SDR_MAX_DEVICES]{}; /* logical index of owning handle */
};

inline void rtl_claim_clear(RtlClaimTable *t)
{
    if (t == nullptr) {
        return;
    }
    *t = RtlClaimTable{};
}

inline int rtl_claim_find(const RtlClaimTable *t, uint8_t addr)
{
    if (t == nullptr || addr == 0) {
        return -1;
    }
    for (size_t i = 0; i < ESP_RTL_SDR_MAX_DEVICES; ++i) {
        if (t->addr[i] == addr) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/** True if addr is owned by a slot other than except_owner (0xFF = any owner). */
inline bool rtl_claim_taken_by_other(const RtlClaimTable *t, uint8_t addr, uint8_t except_owner)
{
    const int i = rtl_claim_find(t, addr);
    if (i < 0) {
        return false;
    }
    return t->owner[static_cast<size_t>(i)] != except_owner;
}

inline size_t rtl_claim_count(const RtlClaimTable *t)
{
    if (t == nullptr) {
        return 0;
    }
    size_t n = 0;
    for (size_t i = 0; i < ESP_RTL_SDR_MAX_DEVICES; ++i) {
        if (t->addr[i] != 0) {
            ++n;
        }
    }
    return n;
}

/**
 * Claim usb_addr for owner_slot. Fails if another owner already holds it.
 * Re-claiming the same addr for the same owner is OK.
 */
inline bool rtl_claim_try(RtlClaimTable *t, uint8_t addr, uint8_t owner_slot)
{
    if (t == nullptr || addr == 0 || owner_slot >= ESP_RTL_SDR_MAX_DEVICES) {
        return false;
    }
    const int existing = rtl_claim_find(t, addr);
    if (existing >= 0) {
        return t->owner[static_cast<size_t>(existing)] == owner_slot;
    }
    for (size_t i = 0; i < ESP_RTL_SDR_MAX_DEVICES; ++i) {
        if (t->addr[i] == 0) {
            t->addr[i] = addr;
            t->owner[i] = owner_slot;
            return true;
        }
    }
    return false;
}

inline void rtl_claim_release(RtlClaimTable *t, uint8_t addr, uint8_t owner_slot)
{
    if (t == nullptr || addr == 0) {
        return;
    }
    for (size_t i = 0; i < ESP_RTL_SDR_MAX_DEVICES; ++i) {
        if (t->addr[i] == addr && t->owner[i] == owner_slot) {
            t->addr[i] = 0;
            t->owner[i] = 0;
            return;
        }
    }
}

inline void rtl_claim_release_owner(RtlClaimTable *t, uint8_t owner_slot)
{
    if (t == nullptr) {
        return;
    }
    for (size_t i = 0; i < ESP_RTL_SDR_MAX_DEVICES; ++i) {
        if (t->owner[i] == owner_slot && t->addr[i] != 0) {
            t->addr[i] = 0;
            t->owner[i] = 0;
        }
    }
}

inline int rtl_logical_alloc(bool used[ESP_RTL_SDR_MAX_DEVICES])
{
    if (used == nullptr) {
        return -1;
    }
    for (size_t i = 0; i < ESP_RTL_SDR_MAX_DEVICES; ++i) {
        if (!used[i]) {
            used[i] = true;
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline void rtl_logical_free(bool used[ESP_RTL_SDR_MAX_DEVICES], int slot)
{
    if (used == nullptr || slot < 0 || slot >= static_cast<int>(ESP_RTL_SDR_MAX_DEVICES)) {
        return;
    }
    used[slot] = false;
}

inline bool rtl_logical_index_valid(int slot)
{
    return slot >= 0 && slot < static_cast<int>(ESP_RTL_SDR_MAX_DEVICES);
}

inline uint32_t rtl_next_sequence(uint32_t *seq)
{
    if (seq == nullptr) {
        return 0;
    }
    *seq += 1u;
    return *seq;
}

/** Independent receiver configuration snapshot — host tests prove no aliasing. */
struct RtlReceiverConfig {
    uint32_t frequency_hz = 0;
    uint32_t sample_rate_sps = 0;
    int gain_tenth_db = 0;
    uint8_t gain_mode = 0;
    int ppm = 0;
    bool bias_tee = false;
};

inline bool rtl_receiver_config_equal(const RtlReceiverConfig &a, const RtlReceiverConfig &b)
{
    return a.frequency_hz == b.frequency_hz && a.sample_rate_sps == b.sample_rate_sps &&
           a.gain_tenth_db == b.gain_tenth_db && a.gain_mode == b.gain_mode && a.ppm == b.ppm &&
           a.bias_tee == b.bias_tee;
}

/**
 * Guardrail: two live contexts must not share the same USB address.
 * Returns false if a duplicate claimed address is found.
 */
inline bool rtl_claim_no_duplicate_addrs(const RtlClaimTable *t)
{
    if (t == nullptr) {
        return true;
    }
    for (size_t i = 0; i < ESP_RTL_SDR_MAX_DEVICES; ++i) {
        if (t->addr[i] == 0) {
            continue;
        }
        for (size_t j = i + 1; j < ESP_RTL_SDR_MAX_DEVICES; ++j) {
            if (t->addr[j] == t->addr[i]) {
                return false;
            }
        }
    }
    return true;
}
