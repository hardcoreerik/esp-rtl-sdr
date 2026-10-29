#include "rtl_usb_shutdown.hpp"
#include <cstdio>
#include <vector>
struct Host {
    bool freed = false, joined = false;
    int free_error = ESP_OK, join_error = ESP_OK, uninstall_error = ESP_OK;
    std::vector<int> calls;
    int free_devices() { calls.push_back(1); if (!free_error) freed = true; return free_error; }
    void stop_events() { calls.push_back(2); }
    int join_events() { calls.push_back(3); if (!join_error) joined = true; return join_error; }
    int uninstall() { calls.push_back(4); return freed && joined ? uninstall_error : ESP_ERR_INVALID_STATE; }
};
int main() {
    int failures = 0;
    auto check = [&](bool ok) { if (!ok) { ++failures; std::puts("FAIL shutdown ordering or error propagation"); } };
    Host normal; check(rtl_usb::shutdown_host(normal) == ESP_OK);
    check(normal.calls == std::vector<int>({1,2,3,4}));
    Host timeout; timeout.free_error = ESP_ERR_TIMEOUT;
    check(rtl_usb::shutdown_host(timeout) == ESP_ERR_TIMEOUT);
    check(timeout.calls == std::vector<int>({1}));
    Host free_error; free_error.free_error = ESP_ERR_INVALID_STATE;
    check(rtl_usb::shutdown_host(free_error) == ESP_ERR_INVALID_STATE);
    check(free_error.calls == std::vector<int>({1}));
    Host join; join.join_error = ESP_ERR_TIMEOUT;
    check(rtl_usb::shutdown_host(join) == ESP_ERR_TIMEOUT);
    check(join.calls == std::vector<int>({1,2,3}));
    Host uninstall; uninstall.uninstall_error = ESP_ERR_INVALID_STATE;
    check(rtl_usb::shutdown_host(uninstall) == ESP_ERR_INVALID_STATE);
    std::printf("shutdown failures=%d\n", failures);
    return failures ? 1 : 0;
}
