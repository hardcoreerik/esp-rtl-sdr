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
 * - Forbidden: install/uninstall/start/stop/retune/reset **from the event
 *   callback task** on the same handle (ERR_REENTRANT). Other tasks may call
 *   setters while a callback is running; the guard is the calling task, not a
 *   handle-global "callback in progress" flag.
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
#define ESP_RTL_SDR_VERSION_MINOR 7
#define ESP_RTL_SDR_VERSION_PATCH 9

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

/** Human-readable version, e.g. "0.7.0". Never NULL; static storage. */
const char *esp_rtl_sdr_get_version_string(void);

/* -------------------------------------------------------------------------- */
/* Errors (component-specific; also use standard esp_err_t)                   */
/* -------------------------------------------------------------------------- */

/** Base for component errors (avoid clash with IDF core). */
#define ESP_RTL_SDR_ERR_BASE           0x12A00

#define ESP_RTL_SDR_ERR_NO_DEVICE      (ESP_RTL_SDR_ERR_BASE + 1)
/** Device present but not an accepted profile (legacy name NOT_V4). */
#define ESP_RTL_SDR_ERR_NOT_V4         (ESP_RTL_SDR_ERR_BASE + 2)
/** Preferred alias for NOT_V4 — unknown / unsupported dongle. */
#define ESP_RTL_SDR_ERR_UNSUPPORTED_DEVICE ESP_RTL_SDR_ERR_NOT_V4
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
/** Device index or serial selection out of range / not found. */
#define ESP_RTL_SDR_ERR_BAD_DEVICE     (ESP_RTL_SDR_ERR_BASE + 15)

/** Convert esp_err_t (including component codes) to a stable string. Never NULL. */
const char *esp_rtl_sdr_err_to_name(esp_err_t err);

/* -------------------------------------------------------------------------- */
/* Constants (policy)                                                         */
/* -------------------------------------------------------------------------- */

/** Official Blog V4 USB identity (measured). */
#define ESP_RTL_SDR_USB_VID            0x0BDA
#define ESP_RTL_SDR_USB_PID            0x2838

/**
 * Named sample rates (Hz) — convenience macros. Any in-spec rate is accepted
 * after quantize (see docs/RATES.md). Programming uses RTL2832 ratio formula.
 */
#define ESP_RTL_SDR_RATE_250K          250000u
#define ESP_RTL_SDR_RATE_256K          256000u
#define ESP_RTL_SDR_RATE_960K          960000u   /**< P4 continuous path (provenance) */
#define ESP_RTL_SDR_RATE_1024K         1024000u
#define ESP_RTL_SDR_RATE_1800K         1800000u
#define ESP_RTL_SDR_RATE_2048K         2048000u  /**< P4 ADS-B path (provenance) */
#define ESP_RTL_SDR_RATE_2400K         2400000u  /**< PC clean-room capture rate */
#define ESP_RTL_SDR_RATE_2560K         2560000u  /**< vendor "stable" ceiling (Blog V4 DS) */
#define ESP_RTL_SDR_RATE_3200K         3200000u  /**< max; drops expected */

/**
 * Hardware sample-rate windows (RTL2832U resampler + ecosystem practice).
 * Outside these, is_rate_supported / quantize reject.
 *
 * Low band: **> 225 kHz … 300 kHz** (desktop librtlsdr rejects rate <= 225000;
 * at exactly 225000 the 28-bit ratio field masks to 0 and cannot be programmed).
 * High band: 900 kHz … 3.2 MHz. Gap 300001–899999 is unstable / rejected.
 */
#define ESP_RTL_SDR_RATE_LOW_MIN_HZ    225001u
#define ESP_RTL_SDR_RATE_LOW_MAX_HZ    300000u
#define ESP_RTL_SDR_RATE_HIGH_MIN_HZ   900000u
#define ESP_RTL_SDR_RATE_HIGH_MAX_HZ   3200000u
/** Vendor stable IQ bandwidth claim (Blog V4 datasheet). */
#define ESP_RTL_SDR_RATE_STABLE_MAX_HZ 2560000u

/** RTL2832 crystal for resampler math (Hz). */
#define ESP_RTL_SDR_XTAL_HZ            28800000u

/** ppm correction range for set_freq_correction (software LO offset). */
#define ESP_RTL_SDR_PPM_MIN            (-200)
#define ESP_RTL_SDR_PPM_MAX            (200)

/** Max simultaneous candidate dongles tracked for multi-device APIs. */
#define ESP_RTL_SDR_MAX_DEVICES        8

/** Max entries in a rate passport probe. */
#define ESP_RTL_SDR_PASSPORT_MAX_ENTRIES 12

/** Default dwell per rate during passport probe (ms). */
#define ESP_RTL_SDR_PASSPORT_DEFAULT_DWELL_MS 1500u

/** Named preset LO frequencies (Hz) — keep in sync with implementation. */
#define ESP_RTL_SDR_PRESET_KZEL_HZ     96100000u
#define ESP_RTL_SDR_PRESET_NOAA_HZ     162400000u

/**
 * Frequency policy (Hz) — Blog V4 full advertised span (public DS / product page).
 * Below HF_UPCONV_LO_HZ the driver programs the R828D at RF+28.8 MHz (built-in
 * SA612 upconverter) and selects the HF triplexer input.
 */
#define ESP_RTL_SDR_FREQ_MIN_HZ        500000u
#define ESP_RTL_SDR_FREQ_MAX_HZ        1766000000u
/** Built-in HF upconverter LO (Blog V4 public: SA612 @ 28.8 MHz). */
#define ESP_RTL_SDR_HF_UPCONV_LO_HZ    28800000u
/** Triplexer band edges (public V4 product page): HF | VHF | UHF+. */
#define ESP_RTL_SDR_BAND_VHF_MIN_HZ    28800000u
#define ESP_RTL_SDR_BAND_UHF_MIN_HZ    250000000u
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
    /** start() in progress (API locked only for transition; USB work outside). */
    ESP_RTL_SDR_STATE_STARTING = 5,
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
    ESP_RTL_SDR_EVT_HEALTH = 9,     /**< payload: health_info (snapshot) */
    ESP_RTL_SDR_EVT_PASSPORT_PROGRESS = 10, /**< payload: passport_entry */
    ESP_RTL_SDR_EVT_PASSPORT_DONE = 11,     /**< payload: rate_passport */
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
    ESP_RTL_SDR_CAP_BIAS_TEE = 1u << 5,     /**< measured Blog V4 SYS bias (0.7.5+) */
    ESP_RTL_SDR_CAP_DIRECT_SAMPLING = 1u << 6, /**< reserved; not claimed */
    ESP_RTL_SDR_CAP_IQ_ACQUIRE = 1u << 7,   /**< release_iq_block required */
    ESP_RTL_SDR_CAP_FREQ_CORRECTION = 1u << 8, /**< software ppm LO offset */
    ESP_RTL_SDR_CAP_MULTI_DEVICE = 1u << 9, /**< enumerate / select by index/serial */
    ESP_RTL_SDR_CAP_SYNC_READ = 1u << 10,   /**< blocking read() pull ring */
    ESP_RTL_SDR_CAP_CONTINUOUS_RATE = 1u << 11, /**< any in-window rate + quantize */
    ESP_RTL_SDR_CAP_NEED = 1u << 12,        /**< apply_need() intent presets */
    ESP_RTL_SDR_CAP_HEALTH = 1u << 13,      /**< get_health / EVT_HEALTH */
    ESP_RTL_SDR_CAP_PASSPORT = 1u << 14,    /**< on-device rate passport probe */
    ESP_RTL_SDR_CAP_GAIN = 1u << 15,        /**< measured Blog V4 manual gain (0.7.5+) */
    ESP_RTL_SDR_CAP_DELIVERY_MODE = 1u << 16, /**< config.delivery_mode honored */
    /** Blog V4 HF path: RF&lt;28.8 MHz → tuner LO RF+28.8e6 + triplexer HF input (0.7.7+). */
    ESP_RTL_SDR_CAP_HF_UPCONVERTER = 1u << 17,
    /** Tuner AGC AUTO EP0 (R828D 05/07/0c) — measured 2026-08-26. */
    ESP_RTL_SDR_CAP_GAIN_AUTO = 1u << 18,
    /** RTL2832 digital AGC (demod 0x19) — measured 2026-08-26; not tuner AUTO. */
    ESP_RTL_SDR_CAP_RTL_AGC = 1u << 19,
} esp_rtl_sdr_cap_t;

/**
 * How IQ samples reach the app (0.7.4+).
 * Other events (STARTED, ERROR, RETUNED, HEALTH, …) still use event_cb when set.
 *
 * BOTH (default): EVT_IQ_BLOCK + blocking read() pull ring.
 * CALLBACK: EVT_IQ_BLOCK only — no pull-ring RAM until mode changes.
 * READ: pull ring only — no EVT_IQ_BLOCK (saves callback cost).
 */
typedef enum {
    ESP_RTL_SDR_DELIVERY_BOTH = 0,
    ESP_RTL_SDR_DELIVERY_CALLBACK = 1,
    ESP_RTL_SDR_DELIVERY_READ = 2,
} esp_rtl_sdr_delivery_mode_t;

/**
 * Intent presets — apps describe the mission; driver picks LO/rate defaults.
 * Does not start streaming; call start() / start_hz() after.
 */
typedef enum {
    ESP_RTL_SDR_NEED_FM = 0,         /**< broadcast FM-class: 960k @ preferred LO */
    ESP_RTL_SDR_NEED_ADSB = 1,       /**< 1090 MHz, 2.048 MSPS */
    ESP_RTL_SDR_NEED_WX = 2,         /**< NOAA WX 162.400 MHz, 960k */
    ESP_RTL_SDR_NEED_HF = 3,         /**< HF intent — WWV 10 MHz default; CAP_HF_UPCONVERTER */
    ESP_RTL_SDR_NEED_MAX_STABLE = 4, /**< passport best_stable, else 2.048M */
    ESP_RTL_SDR_NEED_LISTEN = 5,     /**< lowest-drop default: 960k, keep LO */
} esp_rtl_sdr_need_t;

/** Coarse health categories for dashboards (not medical). */
typedef enum {
    ESP_RTL_SDR_HEALTH_UNKNOWN = 0,
    ESP_RTL_SDR_HEALTH_OK = 1,
    ESP_RTL_SDR_HEALTH_USB_STARVING = 2,  /**< host/USB cannot keep effective SPS */
    ESP_RTL_SDR_HEALTH_APP_TOO_SLOW = 3,  /**< consumer drops / ring pressure */
    ESP_RTL_SDR_HEALTH_RF_CLIPPING = 4,   /**< sample range near full-scale */
    ESP_RTL_SDR_HEALTH_RF_WEAK = 5,       /**< very low sample swing */
} esp_rtl_sdr_health_t;

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
 * Health snapshot for EVT_HEALTH / get_health().
 * struct_size must be set by the driver on output.
 */
typedef struct {
    size_t struct_size;
    esp_rtl_sdr_health_t usb;
    esp_rtl_sdr_health_t rf;
    esp_rtl_sdr_health_t overall;
    float efficiency; /**< effective_sps / programmed_sps; 0 if unknown */
    uint32_t effective_sps;
    uint32_t programmed_sps;
    uint32_t overruns;
    uint32_t consumer_drops;
    uint8_t sample_min;
    uint8_t sample_max;
    char advice[96]; /**< short human hint; never empty when overall != OK */
} esp_rtl_sdr_health_info_t;

/** One passport probe row (also EVT_PASSPORT_PROGRESS payload). */
typedef struct {
    uint32_t requested_sps;
    uint32_t exact_sps;
    uint32_t effective_sps;
    uint32_t overruns;
    uint32_t consumer_drops;
    uint8_t sample_min;
    uint8_t sample_max;
    bool stable; /**< efficiency >= min_efficiency_pct / 100 */
    esp_err_t start_err;
} esp_rtl_sdr_passport_entry_t;

/**
 * On-device rate passport: learned truth for *this* host + dongle.
 * Not a claim of universal hardware; re-run after cable/board change.
 */
typedef struct {
    size_t struct_size;
    size_t entry_count;
    esp_rtl_sdr_passport_entry_t entries[ESP_RTL_SDR_PASSPORT_MAX_ENTRIES];
    uint32_t best_stable_sps; /**< 0 if none met efficiency bar */
    uint32_t max_tried_sps;
    uint32_t probe_freq_hz;
    uint32_t dwell_ms;
    bool valid;
} esp_rtl_sdr_rate_passport_t;

/** Options for probe_rates(). Always call passport_opts_default first. */
typedef struct {
    size_t struct_size;
    /** LO during probe; 0 = preferred / KZEL. */
    uint32_t frequency_hz;
    /** Per-rate stream time; 0 = DEFAULT_DWELL_MS. */
    uint32_t dwell_ms;
    /** Stable if 100*effective/exact >= this; 0 = 95. */
    uint32_t min_efficiency_pct;
    /**
     * true: only recommended named rates.
     * false: recommended + a few extra high-band steps (still bounded).
     */
    bool recommended_only;
} esp_rtl_sdr_passport_opts_t;

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
 * struct_size: set to sizeof(esp_rtl_sdr_config_t) from the header you compiled
 * against. Validation accepts min..sizeof (append-only ABI): smaller sizes get
 * newer trailing fields defaulted. Always call config_default() first when able.
 */
typedef struct {
    size_t struct_size; /**< sizeof(esp_rtl_sdr_config_t) at app compile time */
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
    /**
     * IQ delivery path (append-only field — older struct_size keeps BOTH).
     * See esp_rtl_sdr_delivery_mode_t. Default BOTH.
     */
    esp_rtl_sdr_delivery_mode_t delivery_mode;
    /**
     * Optional pull-ring capacity in bytes (CU8). 0 = auto: prefer ~4× URB
     * total (min ~192 KiB, max 512 KiB), then shrink to the largest even
     * internal heap block down to 64 KiB if PSRAM/internal cannot take the
     * preferred size (Tab5-class no-PSRAM drop-in). Non-zero is exact and
     * fail-closed (ESP_ERR_NO_MEM, no shrink). Only allocated when the mode
     * uses read() (lazy on first push/read).
     */
    size_t pull_ring_bytes;
} esp_rtl_sdr_config_t;

typedef struct {
    size_t struct_size; /**< sizeof(esp_rtl_sdr_stream_config_t) at app compile time */
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

/**
 * True if sample_rate_sps is within hardware windows and quantizes to a legal
 * RTL2832 ratio (continuous rates — not only named macros).
 */
bool esp_rtl_sdr_is_rate_supported(uint32_t sample_rate_sps);

/**
 * Map requested SPS to the nearest exact rate the resampler can program.
 * @return true and *out_exact_sps set on success; false if out of window / bad.
 */
bool esp_rtl_sdr_quantize_sample_rate(uint32_t requested_sps, uint32_t *out_exact_sps);

/**
 * Copy the *recommended* sample rates (named presets) into out_rates.
 * Continuous in-window rates are also accepted by set_sample_rate / start even
 * when not in this list. Query size with max_count == 0.
 */
esp_err_t esp_rtl_sdr_get_supported_rates(uint32_t *out_rates,
                                             size_t max_count,
                                             size_t *out_count);

/**
 * Clamp and quantize frequency to driver policy.
 * Returns false if out of absolute range or out_hz is NULL.
 * Range: FREQ_MIN_HZ (500 kHz) … FREQ_MAX_HZ (with CAP_HF_UPCONVERTER).
 */
bool esp_rtl_sdr_normalize_frequency(uint32_t in_hz, uint32_t *out_hz);

/**
 * True if RF is in the Blog V4 HF upconverter band (RF < 28.8 MHz).
 * Public product page: SA612 LO 28.8 MHz; software adds the offset.
 */
bool esp_rtl_sdr_frequency_uses_hf_upconverter(uint32_t rf_hz);

/**
 * Map user RF (Hz) to R828D tune frequency (Hz).
 * HF band: RF + HF_UPCONV_LO_HZ; otherwise RF unchanged (then IF offset applied in PLL).
 */
uint32_t esp_rtl_sdr_tuner_frequency_hz(uint32_t rf_hz);

/**
 * Resolve preset LO in Hz. For CUSTOM_HZ returns ESP_ERR_INVALID_ARG
 * (caller must supply frequency_hz). Named presets always succeed.
 */
esp_err_t esp_rtl_sdr_preset_frequency_hz(esp_rtl_sdr_preset_t preset,
                                             uint32_t *out_hz);

/** Capability bitmask for this binary (see esp_rtl_sdr_cap_t). */
uint32_t esp_rtl_sdr_get_capabilities(void);

/** True if mode emits EVT_IQ_BLOCK for bulk IQ. */
bool esp_rtl_sdr_delivery_mode_uses_callback_iq(esp_rtl_sdr_delivery_mode_t mode);

/** True if mode fills the sync-read pull ring (esp_rtl_sdr_read). */
bool esp_rtl_sdr_delivery_mode_uses_read(esp_rtl_sdr_delivery_mode_t mode);

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
 *
 * Safe hot path: drains bulk URBs, EP0 tune, resubmits (never EP0 mid-bulk).
 *
 * Threading:
 * - From an app task: applies on the calling task (may block briefly while URBs drain).
 * - From the event callback (e.g. EVT_IQ_BLOCK): **queues** only and returns ESP_OK;
 *   the delivery task applies later and emits EVT_RETUNED. Does **not** return
 *   ERR_REENTRANT — this is intentional async retune (0.7.3+).
 *
 * Coalescing: a newer retune while one is pending/in-flight replaces the target LO.
 *
 * @return ESP_OK if accepted (applied now, or queued for async apply);
 *         ERR_NOT_STREAMING / BAD_FREQ / FAULT / TIMEOUT otherwise.
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
 * Requires delivery_mode BOTH or READ (default BOTH). Returns
 * ERR_UNSUPPORTED for CALLBACK-only mode.
 * Works while STREAMING; pull ring is allocated lazily on first use when needed.
 * @param out_buf  Destination; must be non-NULL.
 * @param max_bytes  Capacity; should be even (odd truncated down).
 * @param timeout_ms  0 = return immediately with whatever is buffered.
 * @param out_bytes  Required; set to bytes copied (0 on timeout with empty buffer).
 * @return ESP_OK if any bytes copied, ERR_TIMEOUT if none within timeout while
 *         streaming, ERR_NOT_STREAMING if idle, ERR_UNSUPPORTED if mode is
 *         CALLBACK-only, other errors as usual.
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

/* -------------------------------------------------------------------------- */
/* Phase 2 — ppm correction + multi-device                                    */
/* -------------------------------------------------------------------------- */

/**
 * Set crystal / LO correction in parts-per-million (software offset).
 * Applied as: tune_hz = request_hz + request_hz * ppm / 1e6 (integer).
 * Range: [ESP_RTL_SDR_PPM_MIN, ESP_RTL_SDR_PPM_MAX]. Default 0.
 * Takes effect on next tune / retune / set_center_freq.
 */
esp_err_t esp_rtl_sdr_set_freq_correction(esp_rtl_sdr_handle_t handle, int ppm);

/** Get current ppm correction. */
esp_err_t esp_rtl_sdr_get_freq_correction(esp_rtl_sdr_handle_t handle, int *out_ppm);

/**
 * Rescan USB for accepted profile devices (Blog V4 identity today).
 * Updates internal candidate list used by get_device_count / select_*.
 * Does not close the currently open device unless it vanished.
 */
esp_err_t esp_rtl_sdr_refresh_device_list(esp_rtl_sdr_handle_t handle);

/** Number of accepted candidates after last refresh (or install scan). */
esp_err_t esp_rtl_sdr_get_device_count(esp_rtl_sdr_handle_t handle, size_t *out_count);

/**
 * Snapshot candidate info at index [0, count).
 * Does not change which device is open.
 */
esp_err_t esp_rtl_sdr_get_device_at(esp_rtl_sdr_handle_t handle, size_t index,
                                    esp_rtl_sdr_device_info_t *out_info);

/**
 * Select candidate by index for the next claim/start (and open now if idle).
 * Must not be streaming. Returns ERR_BAD_DEVICE if index invalid.
 */
esp_err_t esp_rtl_sdr_select_device(esp_rtl_sdr_handle_t handle, size_t index);

/**
 * Select candidate by serial string (exact match, case-sensitive).
 * Must not be streaming.
 */
esp_err_t esp_rtl_sdr_select_device_serial(esp_rtl_sdr_handle_t handle, const char *serial);

/* -------------------------------------------------------------------------- */
/* Beyond rates — intent, health, passport (0.7)                             */
/* -------------------------------------------------------------------------- */

/**
 * Apply a mission intent: sets preferred LO + sample rate (quantized).
 * Does not start streaming. NEED_HF stores HF LO; full upconverter CAP still open.
 * NEED_MAX_STABLE uses last successful passport best_stable_sps when valid.
 */
esp_err_t esp_rtl_sdr_apply_need(esp_rtl_sdr_handle_t handle, esp_rtl_sdr_need_t need);

/** Snapshot USB/RF health from live metrics. Safe while streaming. */
esp_err_t esp_rtl_sdr_get_health(esp_rtl_sdr_handle_t handle,
                                 esp_rtl_sdr_health_info_t *out_health);

void esp_rtl_sdr_passport_opts_default(esp_rtl_sdr_passport_opts_t *opts);

/**
 * On-device rate passport: stream each candidate rate for dwell_ms, measure
 * effective SPS / drops, fill out_passport. Must not already be streaming.
 * Blocks for ~entry_count * dwell_ms. Emits PASSPORT_PROGRESS / DONE.
 * Stores passport on the handle for NEED_MAX_STABLE.
 */
esp_err_t esp_rtl_sdr_probe_rates(esp_rtl_sdr_handle_t handle,
                                  const esp_rtl_sdr_passport_opts_t *opts,
                                  esp_rtl_sdr_rate_passport_t *out_passport);

/**
 * Copy last passport from handle (from probe_rates). valid=false if never probed.
 */
esp_err_t esp_rtl_sdr_get_rate_passport(esp_rtl_sdr_handle_t handle,
                                        esp_rtl_sdr_rate_passport_t *out_passport);

/* -------------------------------------------------------------------------- */
/* Phase 3 surface — gain / bias (measured Blog V4 manual + bias-T)           */
/* -------------------------------------------------------------------------- */

/**
 * Tuner gain mode. MANUAL = measured ladder (CAP_GAIN). AUTO = measured
 * R828D AGC trio (CAP_GAIN_AUTO, 0.7.8+). Default get() is AUTO until the
 * app forces MANUAL; AUTO EP0 is applied only after the interface is claimed.
 */
typedef enum {
    ESP_RTL_SDR_GAIN_MODE_AUTO = 0,
    ESP_RTL_SDR_GAIN_MODE_MANUAL = 1,
} esp_rtl_sdr_gain_mode_t;

/**
 * Set tuner gain mode. Requires claimed interface (after start).
 * AUTO queues measured 05/07/0c AGC ON (CAP_GAIN_AUTO). MANUAL restores the
 * last ladder step (or 0.0 dB if never set). Same mode twice is a no-op.
 * While streaming, EP0 runs on the delivery task after a bulk pause (async).
 * ESP_OK means the request was accepted, not that the dongle ACKed registers.
 * set_tuner_gain() forces MANUAL. Not the RTL digital AGC (see set_rtl_agc).
 */
esp_err_t esp_rtl_sdr_set_tuner_gain_mode(esp_rtl_sdr_handle_t handle,
                                          esp_rtl_sdr_gain_mode_t mode);

/**
 * Last **requested** gain mode (default AUTO). Not a tuner register readback.
 * May read AUTO before any AUTO EP0 has been sent (preference only).
 */
esp_err_t esp_rtl_sdr_get_tuner_gain_mode(esp_rtl_sdr_handle_t handle,
                                          esp_rtl_sdr_gain_mode_t *out_mode);

/**
 * Manual gain in tenths of dB (e.g. 496 = 49.6 dB). Applies nearest measured
 * Blog V4 step (0.0…49.6 dB ladder). Requires claimed interface (after start).
 * Streaming: queued on the delivery task (async). ESP_OK = accepted request.
 */
esp_err_t esp_rtl_sdr_set_tuner_gain(esp_rtl_sdr_handle_t handle, int gain_tenth_db);

/**
 * Last requested / last accepted ladder step (tenths dB); 0 if never set.
 * Software shadow — not an I2C/EP0 read of R828D registers.
 */
esp_err_t esp_rtl_sdr_get_tuner_gain(esp_rtl_sdr_handle_t handle, int *out_gain_tenth_db);

/**
 * Copy measured manual gains (tenths dB). Size-query: max_count==0 sets *out_count
 * to full ladder length (28 steps for Blog V4 measured table).
 */
esp_err_t esp_rtl_sdr_get_tuner_gains(esp_rtl_sdr_handle_t handle, int *out_gains_tenth_db,
                                      size_t max_count, size_t *out_count);

/**
 * Bias-T enable via measured Blog V4 SYS EP0 (lab 2026-08-12).
 * Requires claimed interface (after start). Multimeter DC not yet recorded.
 * Streaming: async sideband queue. get_bias_tee() is last requested preference.
 */
esp_err_t esp_rtl_sdr_set_bias_tee(esp_rtl_sdr_handle_t handle, bool enable);
esp_err_t esp_rtl_sdr_get_bias_tee(esp_rtl_sdr_handle_t handle, bool *out_enable);

/**
 * RTL2832 digital AGC (SDR# "RTL AGC") — measured demod 0x19 ON=0x25 OFF=0x05.
 * Requires CAP_RTL_AGC and a claimed interface. Independent of tuner AUTO.
 * Additive 0.7.8; default off. Same async sideband rules as gain.
 * set: ESP_OK = request accepted. get: last requested shadow, not demod readback.
 */
esp_err_t esp_rtl_sdr_set_rtl_agc(esp_rtl_sdr_handle_t handle, bool enable);
esp_err_t esp_rtl_sdr_get_rtl_agc(esp_rtl_sdr_handle_t handle, bool *out_enable);

#ifdef __cplusplus
}
#endif
