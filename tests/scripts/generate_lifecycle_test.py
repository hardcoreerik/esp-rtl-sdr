"""Generate a host harness around actual session and smoke implementations.

Only platform calls are mocked. This does not reproduce ESP-IDF's HCD or RF.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
source = (root / "src/esp_rtl_sdr.cpp").read_text()
start = source.index("struct SessionShutdown {")
end = source.index("static esp_err_t start_usb_stack", start)
actual = source[start:end]
acquire_start = source.index("static esp_err_t session_acquire(")
actual = source[acquire_start:start] + actual
prefix = r'''
#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>
#include "rtl_usb_shutdown.hpp"
#define ESP_ERR_NOT_FINISHED 0x10c
#define ESP_RTL_SDR_ERR_TIMEOUT ESP_ERR_TIMEOUT
#define ESP_RTL_SDR_MAX_DEVICES 4
#define ESP_LOGE(...) ((void)0)
#define RTL_LOGI(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
#define ESP_INTR_FLAG_LEVEL1 1
using TickType_t = unsigned;
struct Session {
    int refcount = 1;
    bool host_installed = true, external_host = false;
    bool teardown_pending = false, free_requested = false;
    std::atomic<bool> host_task_run{true}, all_devices_free{false}, host_task_exited{false};
    void *host_task = reinterpret_cast<void *>(1);
    bool bringing_up = false;
    int claims = 0;
    unsigned new_dev_events = 0, gone_events = 0, claim_conflicts = 0;
} s_usb_session;
struct esp_rtl_sdr_handle {
    unsigned logical_index = 0;
    char log_id[12]{};
    struct { bool host_library_already_installed = false; } cfg;
    bool owns_host = false;
    void *host_task = nullptr;
};
static TickType_t ticks;
static int free_result, uninstall_result, uninstall_calls, free_calls, slot_frees;
static int install_result, power_result, create_result, next_slot;
static bool root_unpowered;
static constexpr int kUsbPrio = 1, kUsbCore = 0;
struct usb_host_config_t { int intr_flags; bool root_port_unpowered; };
static void host_lib_task_fn(void *) {}
static int usb_host_install(const usb_host_config_t *cfg) { root_unpowered = cfg->root_port_unpowered; return install_result; }
static int usb_host_lib_set_root_port_power(bool on) { if (!power_result) root_unpowered = !on; return power_result; }
static int xTaskCreatePinnedToCore(void(*)(void*), const char *, int, void *, int, void **out, int) {
    *out = create_result == pdPASS ? reinterpret_cast<void *>(1) : nullptr; return create_result;
}
static void rtl_claim_clear(int *) {}
static int session_alloc_slot() { return next_slot++; }
static bool stall_free, stall_join;
static std::vector<int> calls;
static bool session_lock() { return true; }
static void session_unlock() {}
static void session_free_slot(unsigned) { ++slot_frees; }
static TickType_t xTaskGetTickCount() { return ticks; }
static void vTaskDelay(int) {
    ++ticks;
    if (s_usb_session.host_task_run && !stall_free && ticks >= 3)
        s_usb_session.all_devices_free = true;
    if (!s_usb_session.host_task_run && !stall_join)
        s_usb_session.host_task_exited = true;
}
static int usb_host_device_free_all() { ++free_calls; calls.push_back(1); return free_result; }
static int usb_host_uninstall() {
    ++uninstall_calls; calls.push_back(2);
    if (root_unpowered) return uninstall_result; // no enumeration ever powered after failed task creation
    if (s_usb_session.host_task_run || !s_usb_session.host_task_exited || !s_usb_session.all_devices_free)
        return ESP_ERR_INVALID_STATE;
    return uninstall_result;
}
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while(0)
static void reset() {
    ticks = 0; free_result = ESP_ERR_NOT_FINISHED; uninstall_result = ESP_OK;
    install_result = power_result = ESP_OK; create_result = pdPASS; next_slot = 0; root_unpowered = false;
    uninstall_calls = free_calls = slot_frees = 0; stall_free = stall_join = false; calls.clear();
    s_usb_session.refcount = 1; s_usb_session.host_installed = true;
    s_usb_session.external_host = s_usb_session.teardown_pending = s_usb_session.free_requested = false;
    s_usb_session.host_task_run = true; s_usb_session.host_task_exited = false;
    s_usb_session.all_devices_free = false;
    s_usb_session.bringing_up = false;
}
'''
tests = r'''
int main() {
    reset(); esp_rtl_sdr_handle h;
    CHECK(session_release(&h) == ESP_OK);
    CHECK(calls == std::vector<int>({1,2}));
    CHECK(h.logical_index == 255 && s_usb_session.refcount == 0 && !s_usb_session.host_installed);
    CHECK(slot_frees == 1);
    // Shared ownership: first release must not touch the USB library.
    reset(); h.logical_index = 0; s_usb_session.refcount = 2;
    CHECK(session_release(&h) == ESP_OK && free_calls == 0 && uninstall_calls == 0);
    CHECK(s_usb_session.refcount == 1 && s_usb_session.host_task_run);
    h.logical_index = 1;
    CHECK(session_release(&h) == ESP_OK && uninstall_calls == 1);
    // Externally owned hosts are never torn down by this driver.
    reset(); h.logical_index = 0; s_usb_session.external_host = true;
    CHECK(session_release(&h) == ESP_OK && free_calls == 0 && uninstall_calls == 0);
    // Device-free timeout retains session, handle slot and event processing.
    reset(); h.logical_index = 0; stall_free = true;
    CHECK(session_release(&h) == ESP_ERR_TIMEOUT && uninstall_calls == 0);
    CHECK(s_usb_session.refcount == 1 && s_usb_session.host_installed && s_usb_session.host_task_run);
    CHECK(s_usb_session.teardown_pending && h.logical_index == 0 && slot_frees == 0);
    // An asynchronously completed cleanup is consumed by a retry, not lost.
    s_usb_session.all_devices_free = true; stall_free = false;
    CHECK(session_release(&h) == ESP_OK && free_calls == 1 && uninstall_calls == 1);
    reset(); h.logical_index = 0; stall_join = true;
    CHECK(session_release(&h) == ESP_ERR_TIMEOUT && uninstall_calls == 0);
    CHECK(!s_usb_session.host_task_run && s_usb_session.host_installed && s_usb_session.refcount == 1);
    stall_join = false;
    CHECK(session_release(&h) == ESP_OK && uninstall_calls == 1);
    reset(); h.logical_index = 0; uninstall_result = ESP_ERR_INVALID_STATE;
    CHECK(session_release(&h) == ESP_ERR_INVALID_STATE && s_usb_session.host_installed);
    CHECK(s_usb_session.refcount == 1 && slot_frees == 0);
    uninstall_result = ESP_OK;
    CHECK(session_release(&h) == ESP_OK && uninstall_calls == 2);
    // No-device and failed-enumeration cases cannot be distinguished by this API.
    reset(); h.logical_index = 0; free_result = ESP_OK;
    CHECK(session_release(&h) == ESP_ERR_INVALID_STATE && uninstall_calls == 0);
    CHECK(s_usb_session.host_task_run && s_usb_session.refcount == 1);
    reset(); h.logical_index = 0; free_result = ESP_ERR_INVALID_STATE;
    CHECK(session_release(&h) == ESP_ERR_INVALID_STATE && uninstall_calls == 0);
    CHECK(s_usb_session.host_task_run && slot_frees == 0);
    // No session acquired during a partial install: nothing to release.
    reset(); h.logical_index = 255;
    CHECK(session_release(&h) == ESP_OK && free_calls == 0);
    // First installation and second shared acquisition.
    reset(); s_usb_session.refcount = 0; s_usb_session.host_installed = false;
    CHECK(session_acquire(&h) == ESP_OK && s_usb_session.refcount == 1 && !root_unpowered);
    esp_rtl_sdr_handle second;
    CHECK(session_acquire(&second) == ESP_OK && s_usb_session.refcount == 2);
    reset(); s_usb_session.refcount = 0; s_usb_session.host_installed = false; install_result = ESP_ERR_NO_MEM;
    CHECK(session_acquire(&h) == ESP_ERR_NO_MEM && s_usb_session.refcount == 0 && h.logical_index == 255);
    reset(); s_usb_session.refcount = 0; s_usb_session.host_installed = false; create_result = 0;
    CHECK(session_acquire(&h) == ESP_ERR_NO_MEM && s_usb_session.refcount == 0 && !s_usb_session.host_installed);
    CHECK(root_unpowered && h.logical_index == 255 && uninstall_calls == 1);
    reset(); s_usb_session.refcount = 0; s_usb_session.host_installed = false;
    create_result = 0; uninstall_result = ESP_ERR_INVALID_STATE;
    CHECK(session_acquire(&h) == ESP_ERR_NO_MEM && s_usb_session.host_installed && s_usb_session.refcount == 1);
    CHECK(s_usb_session.teardown_pending && s_usb_session.host_task_exited && !s_usb_session.host_task_run);
    uninstall_result = ESP_OK;
    CHECK(session_release(&h) == ESP_OK && s_usb_session.refcount == 0);
    reset(); s_usb_session.teardown_pending = true;
    CHECK(session_acquire(&h) == ESP_ERR_INVALID_STATE && s_usb_session.refcount == 1 && h.logical_index == 255);
    reset(); s_usb_session.bringing_up = true;
    CHECK(session_acquire(&h) == ESP_ERR_TIMEOUT && s_usb_session.refcount == 1 && h.logical_index == 255);
    reset(); s_usb_session.refcount = 0; s_usb_session.host_installed = false; power_result = ESP_ERR_INVALID_STATE;
    CHECK(session_acquire(&h) == ESP_ERR_INVALID_STATE && s_usb_session.teardown_pending && s_usb_session.refcount == 1);
    std::printf("actual session checks=%d\n", checks); return 0;
}
'''
Path(sys.argv[1]).write_text(prefix + actual + tests)

smoke = (root / "examples/p4_serial_smoke/main/main.cpp").read_text()
def function(text, signature):
    start = text.index(signature)
    end = text.index("\n}", start) + 2
    return text[start:end]

smoke_prefix = r'''
#include <cstdio>
#define ESP_LOGE(...) ((void)0)
static int g_pass, g_fail, g_skip;
static bool power_ok = true;
static int power_calls;
[[maybe_unused]] static bool enable_tab5_usb_power() { ++power_calls; return power_ok; }
'''
smoke_test = r'''
int main() {
    check_usb_power();
#if CONFIG_ESP_RTL_SDR_SMOKE_BOARD_TAB5
    if (power_calls != 1 || g_pass != 1 || g_fail || g_skip) return 1;
    power_ok = false;
    check_usb_power();
    if (power_calls != 2 || g_pass != 1 || g_fail != 1 || g_skip) return 1;
#else
    if (power_calls || g_pass || g_fail || g_skip != 1) return 1;
#endif
    return 0;
}
'''
Path(sys.argv[1]).with_name("test_actual_smoke.cpp").write_text(
    smoke_prefix + "[[maybe_unused]] " + function(smoke, "static void smoke_row(") + "\n"
    + function(smoke, "static void check_usb_power(") + smoke_test)

uninstall = function(source, "esp_err_t esp_rtl_sdr_uninstall(")
template = (root / "tests/host/uninstall_harness.cpp.in").read_text()
Path(sys.argv[1]).with_name("test_actual_uninstall.cpp").write_text(
    template.replace("// ACTUAL_UNINSTALL", uninstall))

stream_start = source.index("esp_err_t esp_rtl_sdr_start(")
allocation_start = min(source.index("        ret = alloc_bulk_pool(handle", stream_start),
                       source.index("        ret = ensure_ring(handle", stream_start))
allocation_end = source.index("        if (handle->delivery_task == nullptr)", allocation_start)
template = (root / "tests/host/start_alloc_harness.cpp.in").read_text()
Path(sys.argv[1]).with_name("test_actual_start_alloc.cpp").write_text(
    template.replace("// ACTUAL_START_ALLOCATIONS", source[allocation_start:allocation_end]))

template = (root / "tests/host/acquisition_io_harness.cpp.in").read_text()
functions = [function(source, name) for name in ["static size_t pull_ring_used(",
    "static size_t pull_ring_space(", "static esp_err_t acquisition_read(",
    "esp_err_t esp_rtl_sdr_configure_acquisition(", "esp_err_t esp_rtl_sdr_read_ex("]]
# Skip the producer's forward declaration.
producer_start = source.index("static void pull_ring_push(", source.index("static size_t pull_ring_space("))
functions.insert(2, function(source[producer_start:], "static void pull_ring_push("))
Path(sys.argv[1]).with_name("test_actual_acquisition_io.cpp").write_text(
    template.replace("// ACTUAL_IO_FUNCTIONS", "\n".join(functions)))

template = (root / "tests/host/recovery_harness.cpp.in").read_text()
Path(sys.argv[1]).with_name("test_actual_recovery.cpp").write_text(
    template.replace("// ACTUAL_RECOVERY", function(source, "static void bulk_recover_stall(")))

template = (root / "tests/host/fifo_harness.cpp.in").read_text()
reset = function(source, "static esp_err_t run_acquisition_fifo_reset(") if "static esp_err_t run_acquisition_fifo_reset(" in source else ""
Path(sys.argv[1]).with_name("test_actual_fifo.cpp").write_text(template.replace("// ACTUAL_FIFO_RESET", reset))
