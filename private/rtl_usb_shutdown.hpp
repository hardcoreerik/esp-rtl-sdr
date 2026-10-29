#pragma once
#include "esp_err.h"
namespace rtl_usb {
// Keep event processing and session ownership alive until cleanup is proven.
template<class Host> esp_err_t shutdown_host(Host &host)
{
    esp_err_t err = host.free_devices();
    if (err != ESP_OK) return err;
    host.stop_events();
    err = host.join_events();
    if (err != ESP_OK) return err;
    return host.uninstall();
}
}
