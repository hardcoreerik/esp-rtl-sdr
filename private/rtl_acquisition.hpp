#pragma once
#include "esp_rtl_sdr_acquisition.h"
#include "resampler_ratio.hpp"
#include <atomic>
#include <cstdint>
#include <limits>

namespace rtl_acq {
constexpr uint32_t loss_causes = ESP_RTL_SDR_ACQ_QUEUE_STARVE | ESP_RTL_SDR_ACQ_PULL_OVERRUN | ESP_RTL_SDR_ACQ_META_OVERRUN;
inline uint64_t integer_sqrt(uint64_t n) {
    uint64_t result=0, bit=uint64_t{1}<<62;
    while (bit>n) bit>>=2;
    while (bit) {
        if (n>=result+bit) { n-=result+bit; result=(result>>1)+bit; }
        else result>>=1;
        bit>>=2;
    }
    return result;
}

inline esp_rtl_sdr_iq_quality_t quality(const uint8_t *data, size_t bytes, uint16_t stride) {
    esp_rtl_sdr_iq_quality_t out{};
    if (!data || !stride || bytes<2) return out;
    uint64_t sum_i=0,sum_q=0,power_i=0,power_q=0;
    out.stride_pairs=stride;
    for (size_t pair=0; pair<bytes/2; pair+=stride) {
        const int i=data[2*pair], q=data[2*pair+1];
        const int di=2*i-255, dq=2*q-255;
        ++out.inspected_pairs;
        out.rail_components+=(i==0 || i==255)+(q==0 || q==255);
        sum_i+=i;sum_q+=q;power_i+=di*di;power_q+=dq*dq;
        const unsigned peak=static_cast<unsigned>((di<0?-di:di)>(dq<0?-dq:dq)?(di<0?-di:di):(dq<0?-dq:dq));
        if (peak>out.peak_component_x2) out.peak_component_x2=static_cast<uint16_t>(peak);
    }
    const uint64_t count=out.inspected_pairs, power=power_i+power_q;
    out.mean_i_milli=static_cast<uint32_t>(sum_i*1000/count);
    out.mean_q_milli=static_cast<uint32_t>(sum_q*1000/count);
    out.mean_i_power_x4=static_cast<uint32_t>(power_i/count);
    out.mean_q_power_x4=static_cast<uint32_t>(power_q/count);
    out.rms_milli=static_cast<uint32_t>(integer_sqrt((power/count)*250000+(power%count)*250000/count));
    return out;
}

inline void clock_estimate(esp_rtl_sdr_sample_meta_t &meta, const esp_rtl_sdr_acquisition_config_t &config) {
    if (!config.clock_estimate_valid) return;
    meta.flags|=ESP_RTL_SDR_ACQ_CLOCK_ESTIMATE;
    meta.estimated_rate_millisps=static_cast<uint64_t>(int64_t(meta.programmed_rate_millisps)+
        int64_t(meta.programmed_rate_millisps)*config.sample_clock_error_ppb/1000000000);
    meta.clock_uncertainty_ppb=config.clock_uncertainty_ppb;
}

inline bool advance(uint64_t &value, uint64_t count) {
    if (count > std::numeric_limits<uint64_t>::max() - value) return false;
    value += count;
    return true;
}

class Timeline {
    esp_rtl_sdr_sample_meta_t state_{};
    uint64_t position_ = 0;
    bool exhausted_ = false;
    bool alignment_uncertain_ = false;
public:
    bool begin(uint64_t receiver, uint32_t frequency, uint32_t requested, uint32_t programmed,
               int32_t ppm, uint32_t reason) {
        if (exhausted_ || !advance(state_.epoch, 1)) { exhausted_ = true; return false; }
        state_.struct_size = sizeof(state_);
        state_.receiver_id = receiver;
        state_.center_frequency_hz = frequency;
        state_.requested_rate_sps = requested;
        state_.programmed_rate_sps = programmed;
        state_.resampler_ratio_register = resampler_ratio_register(programmed);
        const uint32_t realized = resampler_realized_ratio(state_.resampler_ratio_register);
        state_.programmed_rate_millisps = realized ? ((uint64_t(ESP_RTL_SDR_XTAL_HZ)<<22)*1000)/realized : 0;
        state_.frequency_correction_ppm = ppm;
        // Physical continuity between epochs is unknown; never invent a sample count.
        state_.unknown_gap_count = state_.epoch > 1 ? 1 : 0;
        state_.flags = reason;
        position_ = 0;
        alignment_uncertain_ = false;
        return true;
    }
    void interrupt(uint32_t reason) {
        if (!advance(state_.unknown_gap_count, 1)) exhausted_ = true;
        state_.flags |= reason;
    }
    void discard(uint32_t reason) { state_.flags |= reason; }
    esp_rtl_sdr_sample_meta_t observe(size_t bytes, int64_t timestamp) {
        auto out = state_;
        out.sample_index = position_;
        out.block_sample_index = position_;
        out.host_timestamp_us = timestamp;
        out.sample_count = static_cast<uint32_t>(bytes / 2);
        out.block_sample_count = out.sample_count;
        state_.flags &= ~loss_causes;
        if (bytes & 1) alignment_uncertain_ = true;
        if (alignment_uncertain_) {
            out.sample_count=0;
            out.flags |= ESP_RTL_SDR_ACQ_SHORT_TRANSFER | ESP_RTL_SDR_ACQ_UNKNOWN_LOSS;
            return out;
        }
        if (exhausted_ || !advance(position_, out.sample_count)) {
            exhausted_ = true; out.sample_count = 0;
        }
        return out;
    }
    uint64_t epoch() const { return state_.epoch; }
};

class Cursor {
    uint64_t epoch_ = 0, end_ = 0, unknown_ = 0;
public:
    esp_rtl_sdr_sample_meta_t deliver(esp_rtl_sdr_sample_meta_t out, uint32_t samples) {
        const bool changed = epoch_ != out.epoch;
        out.flags &= ~(ESP_RTL_SDR_ACQ_KNOWN_LOSS | ESP_RTL_SDR_ACQ_UNKNOWN_LOSS);
        out.known_lost_samples = out.sample_index >= (changed ? 0 : end_)
            ? out.sample_index - (changed ? 0 : end_) : 0;
        if (changed) out.flags |= ESP_RTL_SDR_ACQ_EPOCH_START;
        else out.flags &= ~(ESP_RTL_SDR_ACQ_START | ESP_RTL_SDR_ACQ_RETUNE |
                            ESP_RTL_SDR_ACQ_CONTROL | ESP_RTL_SDR_ACQ_EPOCH_START);
        if (out.known_lost_samples) out.flags |= ESP_RTL_SDR_ACQ_KNOWN_LOSS;
        else out.flags &= ~loss_causes;
        if (out.unknown_gap_count != (changed ? 0 : unknown_)) out.flags |= ESP_RTL_SDR_ACQ_UNKNOWN_LOSS;
        else out.flags &= ~(ESP_RTL_SDR_ACQ_USB_ERROR | ESP_RTL_SDR_ACQ_DISCONNECTED);
        out.sample_count = samples;
        epoch_ = out.epoch; end_ = out.sample_index + samples; unknown_ = out.unknown_gap_count;
        return out;
    }
};

template<size_t N> class Sidecar {
    static_assert(N && (N & (N-1)) == 0, "power-of-two descriptor capacity required");
    struct Span { esp_rtl_sdr_sample_meta_t meta{}; size_t bytes = 0; } spans_[N];
    std::atomic<uint32_t> write_{0}, read_{0};
    size_t offset_ = 0; // consumer-owned
public:
    bool push(const esp_rtl_sdr_sample_meta_t &meta, size_t bytes) {
        const auto w = write_.load(std::memory_order_relaxed);
        if (uint32_t(w - read_.load(std::memory_order_acquire)) >= N || !bytes || (bytes & 1)) return false;
        spans_[w % N] = {meta, bytes};
        write_.store(w+1, std::memory_order_release);
        return true;
    }
    bool peek(size_t max_bytes, esp_rtl_sdr_sample_meta_t &meta, size_t &bytes) const {
        const auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        const auto &span = spans_[r % N];
        bytes = max_bytes < span.bytes-offset_ ? max_bytes : span.bytes-offset_;
        bytes &= ~size_t{1};
        if (!bytes) return false;
        meta = span.meta; meta.sample_index += offset_/2; meta.sample_count = static_cast<uint32_t>(bytes/2);
        return true;
    }
    bool consume(size_t bytes) {
        const auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire) || !bytes || (bytes & 1) || bytes > spans_[r%N].bytes-offset_) return false;
        offset_ += bytes;
        if (offset_ == spans_[r%N].bytes) { offset_=0; read_.store(r+1,std::memory_order_release); }
        return true;
    }
};
}
