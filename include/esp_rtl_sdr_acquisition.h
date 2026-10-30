#pragma once
#include "esp_rtl_sdr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Optional acquisition metadata API. Existing read/callback APIs remain available. */
#define ESP_RTL_SDR_ACQ_START          (1u << 0)
#define ESP_RTL_SDR_ACQ_RETUNE         (1u << 1)
#define ESP_RTL_SDR_ACQ_CONTROL        (1u << 2)
#define ESP_RTL_SDR_ACQ_CONTROL_FAILED (1u << 3)
#define ESP_RTL_SDR_ACQ_KNOWN_LOSS     (1u << 4)
#define ESP_RTL_SDR_ACQ_UNKNOWN_LOSS   (1u << 5)
#define ESP_RTL_SDR_ACQ_USB_ERROR      (1u << 6)
#define ESP_RTL_SDR_ACQ_SHORT_TRANSFER (1u << 7)
#define ESP_RTL_SDR_ACQ_EPOCH_START    (1u << 8)
#define ESP_RTL_SDR_ACQ_DISCONNECTED   (1u << 9)
#define ESP_RTL_SDR_ACQ_CLOCK_ESTIMATE (1u << 10)
#define ESP_RTL_SDR_ACQ_QUEUE_STARVE   (1u << 11)
#define ESP_RTL_SDR_ACQ_PULL_OVERRUN   (1u << 12)
#define ESP_RTL_SDR_ACQ_META_OVERRUN   (1u << 13)

/** All values describe the observed USB block, including on a partial read. */
typedef struct {
    uint32_t inspected_pairs;
    uint32_t rail_components; /**< Components equal to 0 or 255; not proof of an RF overload stage. */
    uint32_t mean_i_milli;    /**< Mean unsigned CU8 code times 1000; midpoint 127500. */
    uint32_t mean_q_milli;
    uint32_t mean_i_power_x4; /**< Mean (2*I-255)^2. */
    uint32_t mean_q_power_x4;
    uint32_t rms_milli;       /**< sqrt(mean((I-127.5)^2+(Q-127.5)^2))*1000. */
    uint16_t peak_component_x2;
    uint16_t stride_pairs;    /**< 1 = complete scan, >1 = sparse estimate, 0 = unavailable. */
} esp_rtl_sdr_iq_quality_t;

typedef struct {
    size_t struct_size;
    bool enabled;            /**< Opt-in, before first start; default false. */
    uint16_t quality_stride_pairs; /**< 0 disables quality; 1..256 inspects every Nth CU8 pair. */
    bool clock_estimate_valid; /**< Application-supplied estimate, never learned from USB arrival times. */
    int32_t sample_clock_error_ppb; /**< Actual rate / programmed rate - 1; +/-1000000 ppb allowed. */
    uint32_t clock_uncertainty_ppb; /**< Application-supplied bound; 0 = unquantified, not perfect accuracy. */
} esp_rtl_sdr_acquisition_config_t;

typedef struct {
    size_t struct_size;
    uint64_t receiver_id;     /**< Boot-local handle instance, never an RF synchronization identity. */
    uint64_t epoch;
    uint64_t sample_index;    /**< Observed complete CU8 pairs in this epoch; excludes unknown physical loss. */
    uint64_t block_sample_index; /**< First pair in the original USB block; quality/timestamp scope. */
    uint64_t known_lost_samples; /**< Observed pairs omitted before this delivery, per consumer channel. */
    uint64_t unknown_gap_count; /**< Interruption markers in this epoch; one outage may produce several, never a sample count. */
    int64_t host_timestamp_us; /**< Client-task observation of transfer completion; not RF sample time. */
    uint32_t sample_count;    /**< Complete CU8 pairs in this returned span. */
    uint32_t block_sample_count; /**< Complete observed pairs in the original USB block. */
    uint32_t center_frequency_hz; /**< 0 if a failed control left hardware configuration uncertain. */
    uint32_t requested_rate_sps;
    uint32_t programmed_rate_sps;
    uint32_t resampler_ratio_register; /**< Stored RTL2832 ratio; bit 28 is supplied by the device. */
    uint64_t programmed_rate_millisps; /**< Nominal crystal/realized-ratio arithmetic, not physical calibration. */
    int32_t frequency_correction_ppm; /**< RF tuning correction, not sample-clock calibration. */
    uint32_t flags;
    uint64_t estimated_rate_millisps; /**< 0 if unknown; application estimate only. */
    uint32_t clock_uncertainty_ppb;
    uint8_t device_id;
    uint8_t usb_addr;
    esp_rtl_sdr_iq_quality_t quality;
} esp_rtl_sdr_sample_meta_t;

/** EVT_IQ_BLOCK_EX payload. Both pointers are borrowed until callback returns. */
typedef struct {
    const uint8_t *data;
    size_t bytes;
    esp_rtl_sdr_sample_meta_t metadata;
} esp_rtl_sdr_iq_block_ex_t;

void esp_rtl_sdr_acquisition_config_default(esp_rtl_sdr_acquisition_config_t *out);
void esp_rtl_sdr_sample_meta_default(esp_rtl_sdr_sample_meta_t *out);
/** Configure once in IDLE before first start. No allocation; actual storage is reserved after USB buffers.
 * NULL/invalid config -> INVALID_ARG; invalid handle -> STALE_HANDLE;
 * streaming/already started -> INVALID_STATE.
 * One owner task; not callable from event callbacks. No borrowed storage retained.
 */
esp_err_t esp_rtl_sdr_configure_acquisition(esp_rtl_sdr_handle_t handle,
                                         const esp_rtl_sdr_acquisition_config_t *config);
/** Copy at most one metadata span, possibly shorter than max_bytes. CU8 pairs only.
 * metadata->struct_size must equal sizeof(*metadata). No allocation, caller owns outputs.
 * Enabled READ/BOTH required; no metadata -> UNSUPPORTED. timeout_ms=0 polls.
 * Captured spans may be drained in IDLE after stop; empty idle -> NOT_STREAMING.
 * Multiple readers are serialized; do not concurrently uninstall the handle.
 * Does not span descriptors/epochs. On error out_bytes=0 and metadata is unchanged.
 */
esp_err_t esp_rtl_sdr_read_ex(esp_rtl_sdr_handle_t handle, uint8_t *out_buf, size_t max_bytes,
                            uint32_t timeout_ms, size_t *out_bytes,
                            esp_rtl_sdr_sample_meta_t *metadata);

#ifdef __cplusplus
}
#endif
