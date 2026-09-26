/**
 * @file esp_rtl_sdr.h
 * @brief esp_rtl_sdr — production public C API (best-in-class contract)
 *
 * Standalone ESP-IDF USB Host client for RTL2832U-class SDR dongles.
 * Blog V4 (R828D) is the measured streaming path. Nooelec NESDR SMArt v5 and
 * Blog V3 are provisional R820T2 streams (I2C 0x34 remap; maintainer-
 * unverified). Transfer sequences are clean-room / measured where stated —
 * this is not a librtlsdr port.
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
 * - One handle owns one logical accepted-profile session (interface 0).
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
#define ESP_RTL_SDR_VERSION_MINOR 8
#define ESP_RTL_SDR_VERSION_PATCH 0
/** 1 while experimental prerelease; 0 for stable X.Y.Z. */
#define ESP_RTL_SDR_VERSION_IS_PRERELEASE 1
/** Token for prerelease suffix (stringized into VERSION_STRING). */
#define ESP_RTL_SDR_VERSION_PRERELEASE rc4

#define ESP_RTL_SDR_VERSION_NUMBER                                      \
    ((ESP_RTL_SDR_VERSION_MAJOR * 10000) +                              \
     (ESP_RTL_SDR_VERSION_MINOR * 100) + ESP_RTL_SDR_VERSION_PATCH)

/**
 * Stringize helpers for version string (single source of truth with macros).
 * Prefer esp_rtl_sdr_get_version_string() at runtime.
 */
#define ESP_RTL_SDR_VERSION_STRING_XSTR(s) #s
#define ESP_RTL_SDR_VERSION_STRING_STR(s) ESP_RTL_SDR_VERSION_STRING_XSTR(s)
#if ESP_RTL_SDR_VERSION_IS_PRERELEASE
#define ESP_RTL_SDR_VERSION_STRING                                      \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_MAJOR) "."    \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_MINOR) "."    \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_PATCH) "-"    \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_PRERELEASE)
#else
#define ESP_RTL_SDR_VERSION_STRING                                      \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_MAJOR) "."    \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_MINOR) "."    \
    ESP_RTL_SDR_VERSION_STRING_STR(ESP_RTL_SDR_VERSION_PATCH)
#endif

/**
 * Packed version: (major << 16) | (minor << 8) | patch.
 * Compare with ESP_RTL_SDR_VERSION_* macros for compile-time checks.
 */
uint32_t esp_rtl_sdr_get_version(void);

/** Human-readable version, e.g. "0.8.0-rc4". Never NULL; static storage. */
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
/**
 * install() refused to touch the USB Host peripheral this boot: the fault
 * guard saw repeated panics during enumeration on prior boots (some 0bda:2838
 * sticks — including at least one RTL-SDR Blog "V3c" that reports the bare
 * factory RTL2838UHIDIR descriptor instead of Blog-branded strings — can
 * STALL EP0 during ESP-IDF's own enumeration before this component's client
 * ever sees a NEW_DEV event; that STALL has triggered a
 * usbh_dev_close/num_ctrl_xfers_inflight assert inside stock ESP-IDF 5.5.4
 * usb_host, i.e. a hard reboot this driver cannot catch or recover from).
 * See esp_rtl_sdr_usb_fault_guard_reset() / esp_rtl_sdr_usb_safe_mode_active().
 */
#define ESP_RTL_SDR_ERR_USB_SAFE_MODE  (ESP_RTL_SDR_ERR_BASE + 16)

/** Convert esp_err_t (including component codes) to a stable string. Never NULL. */
const char *esp_rtl_sdr_err_to_name(esp_err_t err);

/* -------------------------------------------------------------------------- */
/* Constants (policy)                                                         */
/* -------------------------------------------------------------------------- */

/**
 * Shared Realtek RTL2832U USB identity. Many sticks reuse 0bda:2838 —
 * never treat VID/PID alone as Blog V4. Driver matches descriptors / probe.
 */
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
#define ESP_RTL_SDR_RATE_3200K         3200000u  /**< vendor max; drops expected */

/**
 * Hardware sample-rate windows (RTL2832U resampler + ecosystem practice).
 * Outside these, is_rate_supported / quantize reject.
 *
 * Low band: **> 225 kHz … 300 kHz** (desktop librtlsdr rejects rate <= 225000;
 * at exactly 225000 the 28-bit ratio field masks to 0 and cannot be programmed).
 * High band: 900 kHz … 3.2 MHz. Rates above 3.2 MHz lost data in
 * board and PC probes; gap 300001–899999 is rejected.
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
 * Frequency policy (Hz) — driver arithmetic span. Individual device profiles
 * may support a narrower range; start/retune validate the selected profile.
 * Below HF_UPCONV_LO_HZ the driver programs the R828D at RF+28.8 MHz (built-in
 * SA612 upconverter). The Cable-2 route remains selected through exactly 28.8 MHz.
 */
#define ESP_RTL_SDR_FREQ_MIN_HZ        24000u
#define ESP_RTL_SDR_FREQ_MAX_HZ        1766000000u
/** Built-in HF upconverter LO (Blog V4 public: SA612 @ 28.8 MHz). */
#define ESP_RTL_SDR_HF_UPCONV_LO_HZ    28800000u
/** Triplexer edges: HF is <= VHF_MIN_HZ; VHF begins immediately above it. */
#define ESP_RTL_SDR_BAND_VHF_MIN_HZ    28800000u
#define ESP_RTL_SDR_BAND_UHF_MIN_HZ    250000000u
/** Exact-Hz frequency step applied by retune_hz / start. */
#define ESP_RTL_SDR_FREQ_QUANT_HZ      1u

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
    ESP_RTL_SDR_CAP_BIAS_TEE = 1u << 5,     /**< manual bias GPIO control; BlogV3 identity is generic */
    ESP_RTL_SDR_CAP_DIRECT_SAMPLING = 1u << 6, /**< profile supports direct ADC sampling */
    ESP_RTL_SDR_CAP_IQ_ACQUIRE = 1u << 7,   /**< release_iq_block required */
    ESP_RTL_SDR_CAP_FREQ_CORRECTION = 1u << 8, /**< software ppm LO offset */
    ESP_RTL_SDR_CAP_MULTI_DEVICE = 1u << 9, /**< enumerate / select by index/serial */
    ESP_RTL_SDR_CAP_SYNC_READ = 1u << 10,   /**< blocking read() pull ring */
    ESP_RTL_SDR_CAP_CONTINUOUS_RATE = 1u << 11, /**< any in-window rate + quantize */
    ESP_RTL_SDR_CAP_NEED = 1u << 12,        /**< apply_need() intent presets */
    ESP_RTL_SDR_CAP_HEALTH = 1u << 13,      /**< get_health / EVT_HEALTH */
    ESP_RTL_SDR_CAP_PASSPORT = 1u << 14,    /**< on-device rate passport probe */
    ESP_RTL_SDR_CAP_GAIN = 1u << 15,        /**< profile nominal manual gain ladder */
    ESP_RTL_SDR_CAP_DELIVERY_MODE = 1u << 16, /**< config.delivery_mode honored */
    /** V4/V4L board-specific HF path: RF&lt;28.8 MHz → tuner RF+28.8 MHz. */
    ESP_RTL_SDR_CAP_HF_UPCONVERTER = 1u << 17,
    /** Board-specific tuner AGC AUTO EP0 (05/07/0c). */
    ESP_RTL_SDR_CAP_GAIN_AUTO = 1u << 18,
    /** RTL2832 digital AGC (demod 0x19) — measured 2026-08-26; not tuner AUTO. */
    ESP_RTL_SDR_CAP_RTL_AGC = 1u << 19,
    /** Measured tuner filter/IF control at 2.4 MS/s; direct-Q HF excluded. */
    ESP_RTL_SDR_CAP_TUNER_BANDWIDTH = 1u << 20,
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

/**
 * Hardware profile selected by the driver (plug-and-play; no app picker).
 * Identity ≠ tuner family ≠ board front-end. Apps consume capabilities.
 */
typedef enum {
    ESP_RTL_SDR_PROFILE_UNKNOWN = 0,          /**< default until identified / after detach */
    ESP_RTL_SDR_PROFILE_BLOG_V4 = 1,          /**< RTL-SDR Blog V4 / R828D + HF upconverter */
    ESP_RTL_SDR_PROFILE_BLOG_V3 = 2,          /**< Blog V3 / R820T2 provisional stream */
    ESP_RTL_SDR_PROFILE_NOOELEC_SMART_V5 = 3, /**< NESDR SMArt v5 / R820T2-R860 provisional */
    ESP_RTL_SDR_PROFILE_BLOG_V4L = 4,         /**< RTL-SDR Blog V4L (Lite) / R828S */
} esp_rtl_sdr_profile_t;

typedef struct {
    uint16_t vid;
    uint16_t pid;
    char serial[32];
    char manufacturer[48];
    char product[48];
    bool high_speed;
    bool present; /**< false if no accepted profile currently attached */
} esp_rtl_sdr_device_info_t;

typedef struct {
    uint64_t bytes_total;
    uint32_t blocks_total;
    uint32_t short_transfers;
    uint32_t overruns;       /**< USB side could not keep consumer fed / free slots */
    /**
     * DEPRECATED, MIXED UNITS. Historically incremented by 1 when no free
     * IqSlot existed AND by a byte count when the pull ring overwrote old
     * data, then exported as dropped_buffers. A value here can therefore be
     * a count of buffers, a count of bytes, or a sum of both, which made
     * figures like 81920 (= 5 x 16384) look like tens of thousands of lost
     * buffers when they were five ring losses counted in bytes.
     *
     * Kept so existing readers do not break. Use the separated counters
     * below; each has one unit and one cause.
     */
    uint32_t consumer_drops;
    /** No free IqSlot at completion time. Unit: blocks. */
    uint32_t slot_starve_blocks;
    /** Pull ring full, oldest data overwritten. Unit: blocks / bytes. */
    uint32_t ring_overrun_blocks;
    uint64_t ring_overrun_bytes;
    /**
     * pull_ring_push() could not take pull_mux and discarded a completed
     * block. Previously invisible: the USB callback had already done all
     * the work and the data vanished with no counter at all.
     */
    uint32_t pull_lock_miss_blocks;
    uint64_t pull_lock_miss_bytes;
    /** Bytes handed to the application by read(). Unit: bytes. */
    uint64_t bytes_consumed;
    /** Peak pull-ring occupancy in bytes, for pressure visibility. */
    uint32_t ring_high_water;
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
    /**
     * Append-only (0.8 multi-receiver). Older apps that only read the fields
     * above remain valid: the callback payload is the driver's struct.
     * device_id is the handle's logical index (0..MAX_DEVICES-1), NOT USB addr.
     */
    uint8_t device_id;
    int gain_tenth_db;       /**< last requested tuner gain; 0 if unknown */
    uint32_t bandwidth_hz;   /**< occupied IQ bandwidth (= sample_rate); not analog IF */
    uint32_t flags;          /**< ESP_RTL_SDR_IQ_FLAG_* */
} esp_rtl_sdr_iq_block_t;

/** IQ / capture flags (bitmask). Host-time alignment only — not RF coherence. */
#define ESP_RTL_SDR_IQ_FLAG_NONE           0u
#define ESP_RTL_SDR_IQ_FLAG_SHORT_TRANSFER (1u << 0) /**< URB shorter than transfer_bytes */
#define ESP_RTL_SDR_IQ_FLAG_OVERRUN        (1u << 1) /**< free-slot / filled-queue miss */

/** bind_device_index sentinel: claim the first USB address not owned by another handle. */
#define ESP_RTL_SDR_BIND_ANY ((size_t)(-1))

/**
 * Stable identity for one physical receiver. VID/PID/product are NOT unique
 * across identical dongles — use usb_addr + hub_port + serial together.
 */
typedef struct {
    size_t struct_size;
    uint8_t logical_index; /**< handle slot 0..MAX_DEVICES-1; log tag [RTLn] */
    uint8_t usb_addr;      /**< USB device address after enumeration; 0 if none */
    uint8_t parent_addr;   /**< hub device address; 0 = root port or unknown */
    uint8_t hub_port;      /**< downstream port on parent; 0 = unknown/root */
    uint8_t enum_index;    /**< index in last candidate list (0-based) */
    uint16_t vid;
    uint16_t pid;
    char serial[32];
    char manufacturer[48];
    char product[48];
    char usb_path[24]; /**< "root" or "P<addr>.p<port>" */
    bool high_speed;
    bool present;
    esp_rtl_sdr_profile_t profile;
} esp_rtl_sdr_identity_t;

/**
 * Per-buffer / per-handle acquisition metadata for later multi-receiver
 * comparison. Host timestamps are NOT sample-sync or phase-coherent.
 */
typedef struct {
    size_t struct_size;
    uint8_t device_id;
    uint8_t usb_addr;
    uint8_t hub_port;
    uint32_t sequence;
    int64_t host_timestamp_us;
    uint32_t sample_count; /**< CU8 pairs in the associated buffer; 0 if none */
    uint32_t center_frequency_hz;
    uint32_t tuner_frequency_hz; /**< after HF upconverter map when known */
    uint32_t sample_rate_sps;
    uint32_t bandwidth_hz;
    uint8_t gain_mode; /**< esp_rtl_sdr_gain_mode_t stored as u8 for ABI */
    int gain_tenth_db;
    int freq_correction_ppm;
    esp_rtl_sdr_profile_t profile;
    bool bias_tee;
    esp_rtl_sdr_state_t stream_status;
    uint32_t dropped_buffers;
    uint32_t usb_errors;
    uint8_t rms_placeholder;  /**< reserved for a later DSP layer; 0 now */
    uint8_t peak_placeholder; /**< reserved for a later DSP layer; 0 now */
    uint32_t flags;
} esp_rtl_sdr_capture_meta_t;

/** Per-receiver streaming instrumentation (in addition to esp_rtl_sdr_metrics_t). */
typedef struct {
    size_t struct_size;
    uint8_t device_id;
    uint64_t bytes_received;
    uint64_t samples_received; /**< CU8 complex samples = bytes/2 */
    uint32_t usb_transfer_count;
    uint32_t usb_transfer_errors;
    uint32_t usb_timeouts; /**< IDF USB host has no transfer timeouts yet; stays 0 */
    uint32_t short_transfers;
    uint32_t buffer_overruns;
    /** DEPRECATED, MIXED UNITS - mirrors metrics.consumer_drops. */
    uint32_t dropped_buffers;
    uint32_t queue_high_water;
    /* Separated loss accounting. Each has one unit and one cause, so every
     * byte's fate is attributable. */
    uint32_t slot_starve_blocks;
    uint32_t ring_overrun_blocks;
    uint64_t ring_overrun_bytes;
    uint32_t pull_lock_miss_blocks;
    uint64_t pull_lock_miss_bytes;
    uint64_t bytes_consumed;
    uint32_t ring_high_water_bytes;
    uint32_t stream_uptime_ms;
    uint32_t effective_sample_rate;
    int64_t last_transfer_timestamp_us;
} esp_rtl_sdr_stream_stats_t;

/** Shared USB host / hub session counters (not per-dongle). */
typedef struct {
    size_t struct_size;
    uint32_t session_refcount;
    uint32_t claimed_rtl_count;
    uint32_t new_dev_events;
    uint32_t gone_events;
    uint32_t claim_conflicts;
    bool host_installed;
    bool hubs_compiled_in; /**< CONFIG_USB_HOST_HUBS_SUPPORTED when built on IDF */
} esp_rtl_sdr_hub_stats_t;

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
    /**
     * Bind this handle to a candidate index from get_device_at(), or
     * ESP_RTL_SDR_BIND_ANY (default) to claim the first USB address not
     * already owned by another live handle. Append-only: older struct_size
     * keeps BIND_ANY after config_default().
     */
    size_t bind_device_index;
    /** Optional exact serial match. Empty = ignore. */
    char bind_serial[32];
} esp_rtl_sdr_config_t;

typedef struct {
    size_t struct_size; /**< sizeof(esp_rtl_sdr_stream_config_t) at app compile time */
    esp_rtl_sdr_preset_t preset;
    /**
     * Required for CUSTOM_HZ. For named presets, ignored (driver uses fixed LO).
     * Validated and normalized to ESP_RTL_SDR_FREQ_QUANT_HZ.
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
 * Arithmetic range: FREQ_MIN_HZ (24 kHz) … FREQ_MAX_HZ. The selected
 * device profile may reject part of that range.
 */
bool esp_rtl_sdr_normalize_frequency(uint32_t in_hz, uint32_t *out_hz);

/**
 * True when the 28.8 MHz LO offset is required (RF < 28.8 MHz).
 * At exactly 28.8 MHz the HF Cable-2 route is selected without adding the LO.
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

/**
 * Capability bitmask for this binary's Blog V4 feature set (see esp_rtl_sdr_cap_t).
 * For the *attached* dongle, prefer esp_rtl_sdr_get_device_capabilities().
 */
uint32_t esp_rtl_sdr_get_capabilities(void);

/**
 * Active hardware profile for the open device.
 * Returns UNKNOWN for NULL/stale handles or when nothing accepted is attached.
 */
esp_rtl_sdr_profile_t esp_rtl_sdr_get_profile(esp_rtl_sdr_handle_t handle);

/**
 * Capability bitmask for the currently attached profile.
 * Detached / UNKNOWN → 0. Blog V3 adds direct sampling and manual gain;
 * Nooelec omits HF/direct sampling/gain/bias.
 */
uint32_t esp_rtl_sdr_get_device_capabilities(esp_rtl_sdr_handle_t handle);

/** Stable profile name string. Never NULL. */
const char *esp_rtl_sdr_profile_to_name(esp_rtl_sdr_profile_t profile);

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
 *
 * Returns ESP_RTL_SDR_ERR_USB_SAFE_MODE (no handle created, usb_host_install
 * never called) if the fault guard latched after repeated enumeration-time
 * panics — see the macro's doc comment and esp_rtl_sdr_usb_fault_guard_reset().
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

/**
 * True if this boot's install() (or a prior one this session) refused to
 * start the USB Host peripheral because the enumeration fault guard latched.
 * RTC-retained across resets/panics, reset by power loss. Apps can surface
 * this in UI ("USB disabled after repeated crashes — tap to retry").
 */
bool esp_rtl_sdr_usb_safe_mode_active(void);

/**
 * Clear the enumeration fault guard's retained panic counter and safe-mode
 * latch so the next esp_rtl_sdr_install() call will attempt usb_host_install
 * again. Does not affect an already-created handle. Safe to call any time,
 * including when the guard was never latched (no-op then).
 */
esp_err_t esp_rtl_sdr_usb_fault_guard_reset(void);

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
 * Copy device info. present=false if no accepted profile is attached.
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
 *  - ESP_RTL_SDR_ERR_NO_DEVICE if no accepted device is available
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
 * Drains live bulk URBs (same order as retune pause) before freeing the
 * transfer pool; returns ESP_RTL_SDR_ERR_TIMEOUT and leaves FAULT if drain
 * cannot reach live_urbs==0 (pool is not freed while transfers may be in
 * flight - avoids Tab5 HCD assert on stop->start).
 * On TIMEOUT: interface may remain claimed, bulk pool may be kept, and full
 * idle cleanup is not performed (retry stop until drain succeeds).
 * Emits EVT_STOPPED once when leaving STREAMING/STOPPING/FAULT-with-stream.
 */
esp_err_t esp_rtl_sdr_stop(esp_rtl_sdr_handle_t handle, uint32_t timeout_ms);

/**
 * Clear FAULT back to IDLE if hardware allows (no open stream).
 * If still streaming / starting / stopping, returns ERR_BUSY.
 * Refuses while live URBs are outstanding (ERR_BUSY) and keeps FAULT; does
 * not clear state/metrics. Caller should retry esp_rtl_sdr_stop() until drain
 * succeeds, then reset. Clears metrics counters on success.
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
 * Rescan USB for accepted profile devices (Blog V4 / provisional Nooelec / provisional V3).
 * Updates internal candidate list used by get_device_count / select_*.
 * Does not close the currently open device unless it vanished.
 */
esp_err_t esp_rtl_sdr_refresh_device_list(esp_rtl_sdr_handle_t handle);

/** Number of accepted candidates after last refresh (or install scan). */
esp_err_t esp_rtl_sdr_get_device_count(esp_rtl_sdr_handle_t handle, size_t *out_count);

/**
 * Number of devices currently on the USB bus, whatever they are.
 *
 * This is the raw host device list - hubs, non-RTL devices and RTL dongles
 * that have not finished enumerating all count. It is deliberately NOT the
 * accepted-candidate count from esp_rtl_sdr_get_device_count().
 *
 * It exists so a caller can tell "the bus is empty" apart from "the bus is
 * busy enumerating and no RTL device has been accepted yet". Those look
 * identical through the candidate count, and confusing them makes recovery
 * logic tear down enumeration that was still in progress.
 *
 * Needs no handle: it asks the USB host library, not a device.
 */
esp_err_t esp_rtl_sdr_usb_device_count(size_t *out_count);

/**
 * Power-cycle the downstream ports of any external hub on the bus.
 *
 * Toggling the ESP32-P4 ROOT port does nothing useful when the hub is
 * self-powered: it cannot remove power from such a hub, so the devices
 * behind it keep whatever stale state they had. This instead asks the hub
 * itself, over hub-class control requests, to drop and restore power on its
 * own downstream ports - which does reach the dongles.
 *
 * Why it is needed: after a warm MCU reset the devices behind the hub are
 * still powered and still configured from the previous session. IDF creates
 * a device tree node for the first one and then the whole USB host stack
 * goes silent - no descriptor read, no address assignment, no timeout, no
 * retry - and the remaining hub ports are never scanned, so only one device
 * is ever seen and none complete enumeration.
 *
 * @param handle  any installed handle; only its USB client is used
 * @param off_ms  how long to leave port power off (100-1000 is sensible)
 *
 * Returns ESP_ERR_NOT_FOUND when no hub could be opened, which is itself
 * the useful answer - it means this recovery is unavailable and the port
 * power must be removed some other way.
 */
esp_err_t esp_rtl_sdr_hub_port_power_cycle(esp_rtl_sdr_handle_t handle, uint32_t off_ms);

/**
 * Snapshot candidate info at index [0, count).
 * Does not change which device is open.
 */
esp_err_t esp_rtl_sdr_get_device_at(esp_rtl_sdr_handle_t handle, size_t index,
                                    esp_rtl_sdr_device_info_t *out_info);

/**
 * Snapshot identity (USB addr/path/profile) of candidate index without opening it.
 * Does not change which device is open.
 */
esp_err_t esp_rtl_sdr_get_candidate_identity(esp_rtl_sdr_handle_t handle, size_t index,
                                             esp_rtl_sdr_identity_t *out);

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
 * Does not start streaming. NEED_HF stores HF LO; V4 routing is applied when streaming starts.
 * NEED_MAX_STABLE uses last successful passport best_stable_sps when valid.
 */
esp_err_t esp_rtl_sdr_apply_need(esp_rtl_sdr_handle_t handle, esp_rtl_sdr_need_t need);

/**
 * Snapshot USB/RF health from live metrics (lifetime / cumulative from stream
 * start). Safe while streaming.
 *
 * Pre-reader ring overflow and other early drops remain in these counters for
 * the whole stream. For soak / app window measurement use
 * esp_rtl_sdr_health_from_window() on two get_metrics() snapshots instead.
 */
esp_err_t esp_rtl_sdr_get_health(esp_rtl_sdr_handle_t handle,
                                 esp_rtl_sdr_health_info_t *out_health);

/**
 * Windowed metrics from two cumulative get_metrics() snapshots.
 * Pure helper (no handle / mutex). CU8: effective_sps = delta_bytes * 500 / window_ms.
 * Counter underflows clamp to 0.
 */
typedef struct {
    size_t struct_size;
    uint64_t delta_bytes;
    uint32_t delta_short_transfers;
    uint32_t delta_overruns;
    uint32_t delta_consumer_drops;
    uint32_t window_ms;
    uint32_t programmed_sps;
    uint32_t effective_sps; /**< scoped SPS for the window only */
    float efficiency;       /**< effective_sps / programmed_sps; 0 if unknown */
} esp_rtl_sdr_metrics_window_t;

/**
 * Fill delta bytes/overruns/consumer_drops/short_transfers plus scoped SPS and
 * efficiency for [before, after] over window_ms at programmed_sps.
 * out is not modified on failure.
 *
 * @return ESP_OK, or ESP_ERR_INVALID_ARG if a pointer is NULL / window_ms == 0 /
 *         programmed_sps == 0
 */
esp_err_t esp_rtl_sdr_metrics_delta(const esp_rtl_sdr_metrics_t *before,
                                    const esp_rtl_sdr_metrics_t *after,
                                    uint32_t window_ms,
                                    uint32_t programmed_sps,
                                    esp_rtl_sdr_metrics_window_t *out);

/**
 * Windowed USB health from two get_metrics() snapshots.
 * get_health() stays lifetime/cumulative; this is the honest soak/app tool.
 *
 * Fills out_health with window deltas in overruns/consumer_drops, scoped
 * effective_sps/efficiency, and usb/overall in {OK, USB_STARVING, APP_TOO_SLOW,
 * UNKNOWN}. RF is left UNKNOWN (sample swing is not windowed by these counters).
 * Priority matches soak scoring (not get_health()): efficiency < 90% including
 * 0% -> USB_STARVING; else delta consumer_drops > 0 -> APP_TOO_SLOW (no
 * drops-vs-overruns tiebreak); else OK. Delta overruns are reported in
 * overruns but do not alone change the enum (callers may still fail a soak).
 * out_health is not modified on failure.
 *
 * @return ESP_OK, or ESP_ERR_INVALID_ARG if a pointer is NULL / window_ms == 0 /
 *         programmed_sps == 0
 */
esp_err_t esp_rtl_sdr_health_from_window(const esp_rtl_sdr_metrics_t *before,
                                         const esp_rtl_sdr_metrics_t *after,
                                         uint32_t window_ms,
                                         uint32_t programmed_sps,
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
/* Phase 3 surface — gain / bias                                               */
/* -------------------------------------------------------------------------- */

/**
 * Tuner gain mode. MANUAL = measured ladder (CAP_GAIN). AUTO = measured
 * board-specific AGC trio (CAP_GAIN_AUTO). Default get() is AUTO until the
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
 * Returns ESP_RTL_SDR_ERR_UNSUPPORTED while Blog V3 direct sampling bypasses
 * the tuner.
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
 * Manual gain in tenths of dB (e.g. 496 = 49.6 dB). Applies nearest board
 * ladder step; these PC labels are nominal, not calibrated RF gain.
 * Requires claimed interface (after start).
 * Streaming: queued on the delivery task (async). ESP_OK = accepted request.
 * Returns ESP_RTL_SDR_ERR_UNSUPPORTED while Blog V3 direct sampling bypasses
 * the tuner.
 */
esp_err_t esp_rtl_sdr_set_tuner_gain(esp_rtl_sdr_handle_t handle, int gain_tenth_db);

/**
 * Last requested / last accepted ladder step (tenths dB); 0 if never set.
 * Software shadow — not an I2C/EP0 read of R828D registers.
 */
esp_err_t esp_rtl_sdr_get_tuner_gain(esp_rtl_sdr_handle_t handle, int *out_gain_tenth_db);

/**
 * Copy nominal manual gains (tenths dB). Size-query: max_count==0 sets
 * *out_count to full ladder length (V4: 28; V4L/BlogV3: 29).
 */
esp_err_t esp_rtl_sdr_get_tuner_gains(esp_rtl_sdr_handle_t handle, int *out_gains_tenth_db,
                                      size_t max_count, size_t *out_count);

/** Supported tuner widths in Hz for the current (or pending) RF route.
 * Zero means automatic. At 2.4 MS/s only; not an analog passband guarantee.
 * With max_count=0, returns the full count without copying values.
 */
esp_err_t esp_rtl_sdr_get_tuner_bandwidths(esp_rtl_sdr_handle_t handle,
                                            uint32_t *out_hz, size_t max_count,
                                            size_t *out_count);

/** Request a measured tuner width while streaming at 2.4 MS/s.
 * EP0 apply is asynchronous. On write failure the driver restores the prior
 * filter/PLL/demod IF; failed restore enters FAULT without resuming IQ.
 */
esp_err_t esp_rtl_sdr_set_tuner_bandwidth(esp_rtl_sdr_handle_t handle, uint32_t hz);

/** Software shadow: requested may precede applied while EP0 is queued.
 * Check get_last_error()/get_state() for an asynchronous failure.
 */
esp_err_t esp_rtl_sdr_get_tuner_bandwidth_state(esp_rtl_sdr_handle_t handle,
                                                 uint32_t *requested_hz,
                                                 uint32_t *applied_hz);

/**
 * Bias-T manual GPIO control, OFF by default and on stop/new attachment.
 * BlogV3 can be a generic R820T2 stick: its capability does not prove a bias
 * circuit on every board. Warn the user before enabling it; use only with a
 * compatible unpowered accessory. The three tested units measured no-load
 * DC, not safe loaded current. Requires claimed interface (after start).
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

/* -------------------------------------------------------------------------- */
/* Multi-receiver identity / stats / metadata (concurrent handles)            */
/* -------------------------------------------------------------------------- */

void esp_rtl_sdr_identity_default(esp_rtl_sdr_identity_t *out);
void esp_rtl_sdr_capture_meta_default(esp_rtl_sdr_capture_meta_t *out);
void esp_rtl_sdr_stream_stats_default(esp_rtl_sdr_stream_stats_t *out);
void esp_rtl_sdr_hub_stats_default(esp_rtl_sdr_hub_stats_t *out);

/** Format usb_path: "root" or "P<parent>.p<port>". Never writes more than dst_sz. */
void esp_rtl_sdr_format_usb_path(uint8_t parent_addr, uint8_t hub_port,
                                 char *dst, size_t dst_sz);

/**
 * Snapshot identity of the device currently opened by this handle.
 * present=false and usb_addr=0 if nothing is open. out is not modified on failure.
 */
esp_err_t esp_rtl_sdr_get_identity(esp_rtl_sdr_handle_t handle,
                                   esp_rtl_sdr_identity_t *out);

/** Logical index used in [RTLn] logs. 0xFF if handle is stale. */
esp_err_t esp_rtl_sdr_get_logical_index(esp_rtl_sdr_handle_t handle, uint8_t *out_index);

/**
 * Snapshot capture metadata from live handle state (not a queued IQ block).
 * Combine with EVT_IQ_BLOCK sequence/timestamp for per-buffer records.
 */
esp_err_t esp_rtl_sdr_get_capture_meta(esp_rtl_sdr_handle_t handle,
                                       esp_rtl_sdr_capture_meta_t *out);

/** Extended per-device stream counters. Complements get_metrics(). */
esp_err_t esp_rtl_sdr_get_stream_stats(esp_rtl_sdr_handle_t handle,
                                       esp_rtl_sdr_stream_stats_t *out);

/**
 * Shared USB session / hub counters. Handle may be NULL: still reports the
 * process-wide session (zeros if no handle has installed yet).
 */
esp_err_t esp_rtl_sdr_get_hub_stats(esp_rtl_sdr_handle_t handle,
                                    esp_rtl_sdr_hub_stats_t *out);

/**
 * Fill capture metadata from explicit fields (host-testable, no USB).
 * Used by unit tests and by the driver to stamp IQ blocks consistently.
 */
void esp_rtl_sdr_fill_capture_meta(esp_rtl_sdr_capture_meta_t *out,
                                   uint8_t device_id, uint8_t usb_addr, uint8_t hub_port,
                                   uint32_t sequence, int64_t host_timestamp_us,
                                   uint32_t sample_count, uint32_t center_hz,
                                   uint32_t tuner_hz, uint32_t sample_rate_sps,
                                   uint8_t gain_mode, int gain_tenth_db, int ppm,
                                   esp_rtl_sdr_profile_t profile, bool bias_tee,
                                   esp_rtl_sdr_state_t stream_status,
                                   uint32_t dropped_buffers, uint32_t usb_errors,
                                   uint32_t flags);

#ifdef __cplusplus
}
#endif
