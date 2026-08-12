/**
 * @file esp_rtl_sdr.h
 * @brief esp_rtl_sdr — production public C API (best-in-class contract)
 *
 * Standalone ESP-IDF USB Host client for the official RTL-SDR Blog V4
 * (USB 0bda:2838). Transfer sequences are clean-room / measured — this is
 * not a librtlsdr port.
 *
 * ---------------------------------------------------------------------------
 * Design principles (must work every time)
 * ---------------------------------------------------------------------------
 *
 * 1. Fail closed: invalid args, wrong state, and missing hardware never leave
 *    USB half-open. On start failure the handle is IDLE or FAULT, never
 *    "streaming with no URB".
 * 2. Discover before assume: check get_capabilities() and is_rate_supported().
 * 3. Stable growth: config structs carry struct_size; new fields only at end.
 * 4. Thread-safe per handle: all public entry points serialize on one mutex.
 * 5. Callbacks never re-enter lifecycle APIs on the same handle (returns
 *    ERR_REENTRANT). IQ pointers are borrowed until the callback returns
 *    (or release_iq_block when acquire mode is enabled).
 * 6. Idempotent teardown: stop() when idle and uninstall(NULL) always OK.
 *
 * ---------------------------------------------------------------------------
 * Lifecycle (per handle)
 * ---------------------------------------------------------------------------
 *
 *   UNINSTALLED
 *        | install()
 *        v
 *   IDLE  <---------------------------------------------+
 *    | start()                                          |
 *    v                                                  |
 *   STREAMING ---- retune_hz() (queued; no EP0 in bulk) |
 *    |                                                  |
 *    +---- stop()  -------------------------------------+
 *    |
 *    +---- disconnect / fatal error ----> FAULT
 *                                            | reset / stop / uninstall
 *                                            v
 *                                         IDLE / destroyed
 *
 * ---------------------------------------------------------------------------
 * Threading
 * ---------------------------------------------------------------------------
 *
 * - Safe: concurrent get_state / get_metrics / get_device_info / get_last_error
 *   from any task with a live handle.
 * - Safe: start / stop / retune / reset from app tasks (serialized).
 * - Forbidden: install/uninstall/start/stop/retune/reset from inside the
 *   event callback on the same handle (ERR_REENTRANT).
 * - Forbidden: any public API from a USB completion ISR.
 * - IQ / events: delivered from the driver USB owner task (or a dedicated
 *   delivery task). Callbacks must return quickly (no display paint, flash,
 *   or long network blocks).
 *
 * ---------------------------------------------------------------------------
 * Ownership
 * ---------------------------------------------------------------------------
 *
 * - One handle owns one logical V4 session (interface 0).
 * - If host_library_already_installed is false, the driver installs/uninstalls
 *   the USB Host stack for that handle; if true, the app owns install and must
 *   keep the stack alive for the handle lifetime.
 * - Only one stream per handle. Uninstall from a single owner task; do not
 *   call other APIs on a handle concurrent with uninstall.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Version                                                                    */
/* -------------------------------------------------------------------------- */

/** Semantic version of this public header / binary API. */
#define ESP_RTL_SDR_VERSION_MAJOR 0
#define ESP_RTL_SDR_VERSION_MINOR 5
#define ESP_RTL_SDR_VERSION_PATCH 0

#define ESP_RTL_SDR_VERSION_NUMBER                                      \
    ((ESP_RTL_SDR_VERSION_MAJOR * 10000) +                              \
     (ESP_RTL_SDR_VERSION_MINOR * 100) + ESP_RTL_SDR_VERSION_PATCH)

/**
 * Stringize helpers for version string (single source of truth with macros).
 * Prefer esp_rtl_sdr_get_version_string() at runtime.
 */
#define ESP_RTL_SDR_VERSION_STRING_XSTR(s) #s
#define ESP_RTL_SDR_VERSION_STRING_STR(s) ESP_RTL_SDR_VERSION_STRING_XSTR(s)
#define ESP_RTL_SDR_VERSION_STRING                                      \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_MAJOR) "."    \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_MINOR) "."    \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_PATCH)

/**
 * Packed version: (major << 16) | (minor << 8) | patch.
 * Compare with ESP_RTL_SDR_VERSION_* macros for compile-time checks.
 */
uint32_t esp_rtl_sdr_get_version(void);

/** Human-readable version, e.g. "0.4.1". Never NULL; static storage. */
const char *esp_rtl_sdr_get_version_string(void);

/* -------------------------------------------------------------------------- */
/* Errors (component-specific; also use standard esp_err_t)                   */
/* -------------------------------------------------------------------------- */

/** Base for component errors (avoid clash with IDF core). */
#define ESP_RTL_SDR_ERR_BASE           0x12A00

#define ESP_RTL_SDR_ERR_NO_DEVICE      (ESP_RTL_SDR_ERR_BASE + 1)
#define ESP_RTL_SDR_ERR_NOT_V4         (ESP_RTL_SDR_ERR_BASE + 2)
#define ESP_RTL_SDR_ERR_BUSY           (ESP_RTL_SDR_ERR_BASE + 3)
#define ESP_RTL_SDR_ERR_NOT_STREAMING  (ESP_RTL_SDR_ERR_BASE + 4)
#define ESP_RTL_SDR_ERR_BAD_RATE       (ESP_RTL_SDR_ERR_BASE + 5)
#define ESP_RTL_SDR_ERR_BAD_FREQ       (ESP_RTL_SDR_ERR_BASE + 6)
#define ESP_RTL_SDR_ERR_USB            (ESP_RTL_SDR_ERR_BASE + 7)
#define ESP_RTL_SDR_ERR_TIMEOUT        (ESP_RTL_SDR_ERR_BASE + 8)
#define ESP_RTL_SDR_ERR_FAULT          (ESP_RTL_SDR_ERR_BASE + 9)
#define ESP_RTL_SDR_ERR_NOT_READY      (ESP_RTL_SDR_ERR_BASE + 10)
#define ESP_RTL_SDR_ERR_UNSUPPORTED    (ESP_RTL_SDR_ERR_BASE + 11)
#define ESP_RTL_SDR_ERR_STALE_HANDLE   (ESP_RTL_SDR_ERR_BASE + 12)
/** Public API re-entered from event callback on the same handle. */
#define ESP_RTL_SDR_ERR_REENTRANT      (ESP_RTL_SDR_ERR_BASE + 13)
/** Device attached but init/claim not complete. */
#define ESP_RTL_SDR_ERR_NOT_CLAIMED    (ESP_RTL_SDR_ERR_BASE + 14)

/** Convert esp_err_t (including component codes) to a stable string. Never NULL. */
const char *esp_rtl_sdr_err_to_name(esp_err_t err);

/* -------------------------------------------------------------------------- */
/* Constants (policy)                                                         */
/* -------------------------------------------------------------------------- */

/** Official Blog V4 USB identity (measured). */
#define ESP_RTL_SDR_USB_VID            0x0BDA
#define ESP_RTL_SDR_USB_PID            0x2838

/** Measured sustainable sample rate on Tab5 continuous path (Hz). */
#define ESP_RTL_SDR_RATE_960K          960000u
/** Allowlisted higher rates for future HS Ethernet apps (may require eth). */
#define ESP_RTL_SDR_RATE_1024K         1024000u
#define ESP_RTL_SDR_RATE_2048K         2048000u

/** Named preset LO frequencies (Hz) — keep in sync with implementation. */
#define ESP_RTL_SDR_PRESET_KZEL_HZ     96100000u
#define ESP_RTL_SDR_PRESET_NOAA_HZ     162400000u

/** Frequency policy (Hz) for CUSTOM_HZ until calibrated wider bands are proven. */
#define ESP_RTL_SDR_FREQ_MIN_HZ        24000000u
#define ESP_RTL_SDR_FREQ_MAX_HZ        1766000000u
/** Quantization applied by retune_hz / start (Hz). */
#define ESP_RTL_SDR_FREQ_QUANT_HZ      1000u

/**
 * Bulk transfer defaults (bytes). Must be multiple of 512 for HS bulk.
 * Gate 2 default: 6 × 16 KiB (peer-stable multi-URB on ESP32-P4 HS).
 * Apps may override to 3 × 32 KiB via config (legacy Tab5 continuous path).
 */
#define ESP_RTL_SDR_DEFAULT_XFER_BYTES 16384u
#define ESP_RTL_SDR_MIN_XFER_BYTES     512u
#define ESP_RTL_SDR_MAX_XFER_BYTES     262144u
#define ESP_RTL_SDR_DEFAULT_XFER_COUNT 6u
#define ESP_RTL_SDR_MIN_XFER_COUNT     2u
#define ESP_RTL_SDR_MAX_XFER_COUNT     8u
/** Bulk IN endpoint (RTL2832U HS). */
#define ESP_RTL_SDR_BULK_EP_IN         0x81u

/** Default stop wait when caller passes 0 (ms). */
#define ESP_RTL_SDR_DEFAULT_STOP_TIMEOUT_MS 3000u
/** Max accepted control / stop timeout (ms). */
#define ESP_RTL_SDR_MAX_TIMEOUT_MS     30000u

/* -------------------------------------------------------------------------- */
/* Types                                                                      */
/* -------------------------------------------------------------------------- */

typedef struct esp_rtl_sdr_handle *esp_rtl_sdr_handle_t;

typedef enum {
    ESP_RTL_SDR_STATE_UNINSTALLED = 0,
    ESP_RTL_SDR_STATE_IDLE = 1,
    ESP_RTL_SDR_STATE_STREAMING = 2,
    ESP_RTL_SDR_STATE_STOPPING = 3,
    ESP_RTL_SDR_STATE_FAULT = 4,
} esp_rtl_sdr_state_t;

/** Convert state enum to a stable string. Never NULL. */
const char *esp_rtl_sdr_state_to_name(esp_rtl_sdr_state_t state);

/** Allowlisted presets with measured / derived PLL tables. */
typedef enum {
    ESP_RTL_SDR_PRESET_KZEL_96_1 = 0, /**< 96.1 MHz reference (measured) */
    ESP_RTL_SDR_PRESET_NOAA_162_4 = 1, /**< 162.400 MHz reference (measured) */
    ESP_RTL_SDR_PRESET_CUSTOM_HZ = 2,  /**< frequency_hz via driver PLL pack */
} esp_rtl_sdr_preset_t;

typedef enum {
    ESP_RTL_SDR_EVT_ENUMERATED = 1, /**< payload: device_info */
    ESP_RTL_SDR_EVT_READY = 2,      /**< device accepted, not yet streaming */
    ESP_RTL_SDR_EVT_STREAM_STARTED = 3,
    ESP_RTL_SDR_EVT_IQ_BLOCK = 4,   /**< payload: iq_block (borrowed) */
    ESP_RTL_SDR_EVT_STOPPED = 5,
    ESP_RTL_SDR_EVT_ERROR = 6,      /**< payload: error_info */
    ESP_RTL_SDR_EVT_DISCONNECTED = 7,
    ESP_RTL_SDR_EVT_RETUNED = 8,    /**< payload: uint32_t frequency_hz */
} esp_rtl_sdr_event_t;

/**
 * Capability bits returned by esp_rtl_sdr_get_capabilities().
 * Apps must check flags rather than assuming features exist.
 */
typedef enum {
    ESP_RTL_SDR_CAP_STREAM = 1u << 0,       /**< start/stop bulk IQ */
    ESP_RTL_SDR_CAP_RETUNE = 1u << 1,       /**< in-stream retune_hz */
    ESP_RTL_SDR_CAP_HOTPLUG = 1u << 2,      /**< disconnect/reconnect events */
    ESP_RTL_SDR_CAP_METRICS = 1u << 3,      /**< get_metrics live */
    ESP_RTL_SDR_CAP_CUSTOM_HZ = 1u << 4,    /**< CUSTOM_HZ preset */
    ESP_RTL_SDR_CAP_BIAS_TEE = 1u << 5,     /**< reserved; not yet measured */
    ESP_RTL_SDR_CAP_DIRECT_SAMPLING = 1u << 6, /**< reserved; not claimed */
    ESP_RTL_SDR_CAP_IQ_ACQUIRE = 1u << 7,   /**< release_iq_block required */
} esp_rtl_sdr_cap_t;

typedef struct {
    uint16_t vid;
    uint16_t pid;
    char serial[32];
    char manufacturer[48];
    char product[48];
    bool high_speed;
    bool present; /**< false if no V4 currently attached */
} esp_rtl_sdr_device_info_t;

typedef struct {
    uint64_t bytes_total;
    uint32_t blocks_total;
    uint32_t short_transfers;
    uint32_t overruns;       /**< USB side could not keep consumer fed / free slots */
    uint32_t consumer_drops; /**< app too slow (if ring drops newest/oldest) */
    uint8_t sample_min;
    uint8_t sample_max;
    float sample_mean; /**< not double: stable ABI, enough precision */
    uint32_t effective_sps;
    uint32_t frequency_hz;
    uint32_t sample_rate_sps;
    uint32_t last_error;     /**< last component/esp error code */
    uint32_t uptime_ms;      /**< stream uptime while streaming */
} esp_rtl_sdr_metrics_t;

/**
 * Borrowed IQ view. Valid only for the duration of EVT_IQ_BLOCK callback
 * unless iq_acquire_mode is enabled and release_iq_block() is used.
 *
 * Format: interleaved unsigned IQ (I0,Q0,I1,Q1,...) CU8.
 */
typedef struct {
    const uint8_t *data;
    size_t bytes;          /**< always even; multiple of 2 */
    uint32_t sequence;     /**< monotonic per stream, wraps */
    uint32_t frequency_hz; /**< LO after last successful tune */
    uint32_t sample_rate_sps;
    int64_t host_timestamp_us; /**< esp_timer_get_time() style; 0 if unknown */
} esp_rtl_sdr_iq_block_t;

typedef struct {
    esp_err_t code;
    char message[96];
} esp_rtl_sdr_error_info_t;

/**
 * Event callback.
 * @param event  Event kind.
 * @param payload  Event-specific pointer (may be NULL). See event enum.
 * @param user_ctx  Value from config.event_ctx.
 *
 * Must not call install/uninstall/start/stop/retune/reset on the same handle.
 * May call get_state / get_metrics / get_device_info / get_last_error / release_iq_block.
 */
typedef void (*esp_rtl_sdr_event_cb_t)(esp_rtl_sdr_event_t event,
                                          const void *payload,
                                          void *user_ctx);

/**
 * Install configuration.
 *
 * struct_size must be set to sizeof(esp_rtl_sdr_config_t) so future fields
 * remain backward compatible when apps are recompiled against newer headers.
 * Always call esp_rtl_sdr_config_default() before setting fields.
 */
typedef struct {
    size_t struct_size; /**< MUST be sizeof(esp_rtl_sdr_config_t) */
    /** App already called usb_host_install(); driver only registers a client. */
    bool host_library_already_installed;
    /** Bulk URB size (bytes). Must be multiple of 512 for HS. */
    size_t transfer_bytes;
    /** Driver-owned bulk buffers (>= 2). */
    size_t transfer_count;
    uint32_t control_timeout_ms;
    /** Optional. May be NULL if app only uses poll/metrics. */
    esp_rtl_sdr_event_cb_t event_cb;
    void *event_ctx;
    /**
     * If true and CAP_IQ_ACQUIRE is set, EVT_IQ_BLOCK requires
     * esp_rtl_sdr_release_iq_block() before the buffer is reused.
     * Currently ignored (borrow mode only); validate still accepts the flag.
     */
    bool iq_acquire_mode;
    /** Task priority for USB owner (0 = driver default). */
    uint8_t usb_task_priority;
    /** Core affinity: 0 or 1, or 0xFF = no affinity. */
    uint8_t usb_task_core_id;
} esp_rtl_sdr_config_t;

typedef struct {
    size_t struct_size; /**< MUST be sizeof(esp_rtl_sdr_stream_config_t) */
    esp_rtl_sdr_preset_t preset;
    /**
     * Required for CUSTOM_HZ. For named presets, ignored (driver uses fixed LO).
     * Quantized to ESP_RTL_SDR_FREQ_QUANT_HZ.
     */
    uint32_t frequency_hz;
    /**
     * Sample rate (Hz). Must be allowlisted; use ESP_RTL_SDR_RATE_* or
     * esp_rtl_sdr_is_rate_supported().
     */
    uint32_t sample_rate_sps;
    /** 0 = continuous until stop. Else exact CU8 byte bound (even). */
    uint64_t max_bytes;
    /** Soft wall-clock limit for bounded capture; 0 = none. */
    uint32_t timeout_ms;
} esp_rtl_sdr_stream_config_t;

/* -------------------------------------------------------------------------- */
/* Config helpers                                                             */
/* -------------------------------------------------------------------------- */

/**
 * Zero and fill defaults. Always call before setting fields.
 * Sets struct_size correctly. NULL-safe (no-op).
 */
void esp_rtl_sdr_config_default(esp_rtl_sdr_config_t *config);
void esp_rtl_sdr_stream_config_default(esp_rtl_sdr_stream_config_t *stream);

/**
 * Validate config without installing. Returns ESP_OK or ESP_ERR_INVALID_ARG /
 * component error. Does not require a handle. NULL-safe (INVALID_ARG).
 */
esp_err_t esp_rtl_sdr_config_validate(const esp_rtl_sdr_config_t *config);
esp_err_t esp_rtl_sdr_stream_config_validate(const esp_rtl_sdr_stream_config_t *stream);

/** True if sample_rate_sps is on the allowlist for this build. */
bool esp_rtl_sdr_is_rate_supported(uint32_t sample_rate_sps);

/**
 * Copy the allowlisted sample rates into out_rates (up to max_count entries).
 * @param out_rates  Destination; may be NULL if max_count == 0 (query size only).
 * @param max_count  Capacity of out_rates.
 * @param out_count  Required; set to number of rates written (or total if max_count==0).
 * @return ESP_OK, or ESP_ERR_INVALID_ARG if out_count is NULL, or
 *         ESP_ERR_INVALID_SIZE if max_count > 0 but too small for the full list
 *         (still writes min(max_count, total) rates and sets *out_count = total).
 */
esp_err_t esp_rtl_sdr_get_supported_rates(uint32_t *out_rates,
                                             size_t max_count,
                                             size_t *out_count);

/**
 * Clamp and quantize frequency to driver policy.
 * Returns false if out of absolute range or out_hz is NULL.
 */
bool esp_rtl_sdr_normalize_frequency(uint32_t in_hz, uint32_t *out_hz);

/**
 * Resolve preset LO in Hz. For CUSTOM_HZ returns ESP_ERR_INVALID_ARG
 * (caller must supply frequency_hz). Named presets always succeed.
 */
esp_err_t esp_rtl_sdr_preset_frequency_hz(esp_rtl_sdr_preset_t preset,
                                             uint32_t *out_hz);

/** Capability bitmask for this binary (see esp_rtl_sdr_cap_t). */
uint32_t esp_rtl_sdr_get_capabilities(void);

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * Create handle and prepare USB client registration path.
 * On success *out_handle is non-NULL. On failure *out_handle is NULL
 * (always cleared first when out_handle is non-NULL).
 *
 * Does not require a dongle present. Device attach is reported via events
 * as devices attach and detach.
 */
esp_err_t esp_rtl_sdr_install(const esp_rtl_sdr_config_t *config,
                                 esp_rtl_sdr_handle_t *out_handle);

/**
 * Destroy handle. Safe to call with NULL (returns ESP_OK).
 * If streaming, performs stop first (best effort).
 * Always releases resources. Second call on the same pointer after destroy
 * returns STALE_HANDLE (use-after-free is still undefined — do not retain).
 */
esp_err_t esp_rtl_sdr_uninstall(esp_rtl_sdr_handle_t handle);

/* -------------------------------------------------------------------------- */
/* Queries                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * Current state. Returns UNINSTALLED for NULL/stale handles without crashing.
 * Never blocks indefinitely (short lock timeout → FAULT snapshot).
 */
esp_rtl_sdr_state_t esp_rtl_sdr_get_state(esp_rtl_sdr_handle_t handle);

/** Last error stored on handle. STALE_HANDLE for invalid handles. */
esp_err_t esp_rtl_sdr_get_last_error(esp_rtl_sdr_handle_t handle);

/**
 * Copy device info. present=false if no accepted V4 is attached.
 * Thread-safe snapshot. out_info is not modified on failure.
 */
esp_err_t esp_rtl_sdr_get_device_info(esp_rtl_sdr_handle_t handle,
                                         esp_rtl_sdr_device_info_t *out_info);

/**
 * Thread-safe metrics snapshot. out_metrics is not modified on failure.
 * uptime_ms is computed at snapshot time while STREAMING.
 */
esp_err_t esp_rtl_sdr_get_metrics(esp_rtl_sdr_handle_t handle,
                                     esp_rtl_sdr_metrics_t *out_metrics);

/* -------------------------------------------------------------------------- */
/* Streaming                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * Start IQ stream: claim interface, clean-room init, sample rate, tune, bulk IN.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG / BAD_RATE / BAD_FREQ
 *  - ESP_RTL_SDR_ERR_BUSY if already streaming or stopping
 *  - ESP_RTL_SDR_ERR_NO_DEVICE if no V4
 *  - ESP_RTL_SDR_ERR_UNSUPPORTED when the requested path is not built
 *  - ESP_RTL_SDR_ERR_REENTRANT if called from event callback
 *  - ESP_RTL_SDR_ERR_USB / TIMEOUT / FAULT on hardware failure
 *
 * On failure, handle remains IDLE (or FAULT if unrecoverable). Never leaves
 * interface claimed without a matching stop path.
 */
esp_err_t esp_rtl_sdr_start(esp_rtl_sdr_handle_t handle,
                               const esp_rtl_sdr_stream_config_t *stream);

/**
 * Request in-stream retune. Frequency is normalized (quantized/clamped).
 * Implementation queues the request and applies it only when no bulk URB is
 * outstanding (safe for continuous operation).
 *
 * @return ESP_OK if accepted (applied or queued);
 *         ERR_NOT_STREAMING / BAD_FREQ / FAULT / UNSUPPORTED / REENTRANT otherwise.
 */
esp_err_t esp_rtl_sdr_retune_hz(esp_rtl_sdr_handle_t handle, uint32_t frequency_hz);

/**
 * Stop stream and run cleanup. Idempotent if already idle.
 * Blocks up to timeout_ms for USB cleanup (0 = DEFAULT_STOP_TIMEOUT_MS).
 * Emits EVT_STOPPED once when leaving STREAMING/STOPPING/FAULT-with-stream.
 */
esp_err_t esp_rtl_sdr_stop(esp_rtl_sdr_handle_t handle, uint32_t timeout_ms);

/**
 * Clear FAULT back to IDLE if hardware allows (no open stream).
 * If still streaming, returns ERR_BUSY. Clears metrics counters on success.
 */
esp_err_t esp_rtl_sdr_reset(esp_rtl_sdr_handle_t handle);

/**
 * Release an IQ block previously delivered with acquire mode.
 * In borrow mode (default), this is a documented no-op returning ESP_OK
 * when block is non-NULL, so apps can call it unconditionally.
 */
esp_err_t esp_rtl_sdr_release_iq_block(esp_rtl_sdr_handle_t handle,
                                          const esp_rtl_sdr_iq_block_t *block);

/* -------------------------------------------------------------------------- */
/* Desktop-shaped ergonomics (Phase 1) — map closely to librtlsdr mental model */
/* -------------------------------------------------------------------------- */

/**
 * Set preferred / active center frequency (Hz).
 * - IDLE: stores preferred LO for the next start() (and for get_center_freq).
 * - STREAMING: equivalent to retune_hz() (queued safe hot retune).
 * Frequency is normalized (quantized/clamped). 0 is rejected.
 */
esp_err_t esp_rtl_sdr_set_center_freq(esp_rtl_sdr_handle_t handle, uint32_t frequency_hz);

/**
 * Get last applied or preferred center frequency (Hz).
 * 0 if never set and never streamed.
 */
esp_err_t esp_rtl_sdr_get_center_freq(esp_rtl_sdr_handle_t handle, uint32_t *out_hz);

/**
 * Set preferred sample rate (Hz). Must be allowlisted (is_rate_supported).
 * - IDLE: stored for next start() if stream.sample_rate_sps is 0.
 * - STREAMING: returns ERR_BUSY (rate change requires stop/start in Phase 1).
 */
esp_err_t esp_rtl_sdr_set_sample_rate(esp_rtl_sdr_handle_t handle, uint32_t sample_rate_sps);

/**
 * Get last applied or preferred sample rate (Hz).
 */
esp_err_t esp_rtl_sdr_get_sample_rate(esp_rtl_sdr_handle_t handle, uint32_t *out_sps);

/**
 * Blocking pull of interleaved CU8 IQ bytes (sync-read equivalent).
 * Works while STREAMING whether or not an event callback is installed.
 * @param out_buf  Destination; must be non-NULL.
 * @param max_bytes  Capacity; should be even (odd truncated down).
 * @param timeout_ms  0 = return immediately with whatever is buffered.
 * @param out_bytes  Required; set to bytes copied (0 on timeout with empty buffer).
 * @return ESP_OK if any bytes copied, ERR_TIMEOUT if none within timeout while
 *         streaming, ERR_NOT_STREAMING if idle, other errors as usual.
 */
esp_err_t esp_rtl_sdr_read(esp_rtl_sdr_handle_t handle, uint8_t *out_buf, size_t max_bytes,
                           uint32_t timeout_ms, size_t *out_bytes);

/**
 * Convenience: build a stream config from preferred LO/rate (or overrides) and start.
 * If frequency_hz is 0, uses preferred center freq (must be set).
 * If sample_rate_sps is 0, uses preferred sample rate (default 960k after install).
 */
esp_err_t esp_rtl_sdr_start_hz(esp_rtl_sdr_handle_t handle, uint32_t frequency_hz,
                               uint32_t sample_rate_sps);

#ifdef __cplusplus
}
#endif
