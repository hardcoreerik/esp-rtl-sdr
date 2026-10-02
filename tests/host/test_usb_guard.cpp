/*
 * Host tests for private/rtl_usb_guard.hpp: control-transfer limits, configuration-descriptor validation and the
 * failed-enumeration retry timing. No USB stack, FreeRTOS or ESP-IDF.
 */

#include <cstdint>
#include <cstdio>
#include <vector>

#include "rtl_usb_guard.hpp"

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            g_failed++;                                                        \
        } else {                                                               \
            g_passed++;                                                        \
        }                                                                      \
    } while (0)

using namespace rtl_usb;

/* ------------------------------------------------------------------ control transfers */

static void test_ctrl_request_limits()
{
    constexpr size_t kBuffer = 8 + 64; /* the driver's control buffer: setup packet plus 64 data bytes */
    constexpr uint8_t kIn = 0xc0;
    constexpr uint8_t kOut = 0x40;

    /* data stage must fit */
    CHECK(ctrl_request_allowed(kIn, 0x0120, 64, kBuffer));
    CHECK(!ctrl_request_allowed(kIn, 0x0120, 65, kBuffer));
    CHECK(!ctrl_request_allowed(kOut, 0x0610, 65, kBuffer));
    CHECK(ctrl_request_allowed(kOut, 0x0610, 0, kBuffer));
    CHECK(!ctrl_request_allowed(kIn, 0x0120, 0xffff, kBuffer));
    /* a buffer smaller than the setup packet can never carry a request */
    CHECK(!ctrl_request_allowed(kIn, 0x0120, 0, 4));
    CHECK(ctrl_request_allowed(kIn, 0x0120, 0, 8));

    /* I2C passthrough (block 6): reads are capped at 16 bytes, writes are not */
    CHECK(ctrl_request_allowed(kIn, 0x0600, 16, kBuffer));
    CHECK(!ctrl_request_allowed(kIn, 0x0600, 17, kBuffer));
    CHECK(!ctrl_request_allowed(kIn, 0x06ff, 17, kBuffer));
    CHECK(ctrl_request_allowed(kOut, 0x0610, 40, kBuffer));
    /* other blocks are not capped */
    CHECK(ctrl_request_allowed(kIn, 0x0100, 40, kBuffer));
    CHECK(ctrl_request_allowed(kIn, 0x0a00, 40, kBuffer));
    CHECK(ctrl_request_allowed(kIn, 0x0006, 40, kBuffer)); /* low byte 6 is not block 6 */
}

static void test_ctrl_data_bytes()
{
    CHECK(ctrl_data_bytes(0) == 0);
    CHECK(ctrl_data_bytes(-5) == 0);
    CHECK(ctrl_data_bytes(7) == 0);
    CHECK(ctrl_data_bytes(8) == 0);  /* setup packet only */
    CHECK(ctrl_data_bytes(9) == 1);
    CHECK(ctrl_data_bytes(72) == 64);
    CHECK(ctrl_data_bytes(8 + 0xffff) == 0xffff);
    CHECK(ctrl_data_bytes(8 + 0x20000) == 0xffff);  /* clamps */
}

/* ------------------------------------------------------------------ configuration descriptor */

static std::vector<uint8_t> make_config(const std::vector<std::vector<uint8_t>> &parts)
{
    std::vector<uint8_t> body;
    for (const auto &p : parts) body.insert(body.end(), p.begin(), p.end());
    const size_t total = 9 + body.size();
    std::vector<uint8_t> cfg = {9, 0x02, static_cast<uint8_t>(total & 0xff), static_cast<uint8_t>(total >> 8), 1, 1, 0, 0x80, 250};
    cfg.insert(cfg.end(), body.begin(), body.end());
    return cfg;
}

static std::vector<uint8_t> iface(uint8_t number, uint8_t alt)
{
    return {9, 0x04, number, alt, 1, 0xff, 0xff, 0xff, 0};
}

static std::vector<uint8_t> endpoint(uint8_t addr, uint8_t attributes, uint16_t packet)
{
    return {7, 0x05, addr, attributes, static_cast<uint8_t>(packet & 0xff), static_cast<uint8_t>(packet >> 8), 0};
}

static void test_config_descriptor()
{
    const auto good = make_config({iface(0, 0), endpoint(0x81, 0x02, 512)});
    CHECK(config_has_bulk_in(good.data(), good.size(), 0x81));
    CHECK(!config_has_bulk_in(good.data(), good.size(), 0x82));

    /* packet size bounds: 8..512, high bits (transactions per microframe) ignored */
    for (uint16_t packet : {uint16_t(8), uint16_t(64), uint16_t(512), uint16_t(0x0a00)}) {
        const auto cfg = make_config({iface(0, 0), endpoint(0x81, 0x02, packet)});
        const bool expect = (packet & 0x07ff) >= 8 && (packet & 0x07ff) <= 512;
        CHECK(config_has_bulk_in(cfg.data(), cfg.size(), 0x81) == expect);
    }
    const auto too_small = make_config({iface(0, 0), endpoint(0x81, 0x02, 4)});
    CHECK(!config_has_bulk_in(too_small.data(), too_small.size(), 0x81));
    const auto too_big = make_config({iface(0, 0), endpoint(0x81, 0x02, 1024)});
    CHECK(!config_has_bulk_in(too_big.data(), too_big.size(), 0x81));

    /* the endpoint must be bulk (attributes bits 1:0 == 2) */
    const auto interrupt = make_config({iface(0, 0), endpoint(0x81, 0x03, 512)});
    CHECK(!config_has_bulk_in(interrupt.data(), interrupt.size(), 0x81));
    const auto iso = make_config({iface(0, 0), endpoint(0x81, 0x01, 512)});
    CHECK(!config_has_bulk_in(iso.data(), iso.size(), 0x81));

    /* only interface 0, alternate setting 0 counts */
    const auto other_iface = make_config({iface(1, 0), endpoint(0x81, 0x02, 512)});
    CHECK(!config_has_bulk_in(other_iface.data(), other_iface.size(), 0x81));
    const auto other_alt = make_config({iface(0, 1), endpoint(0x81, 0x02, 512)});
    CHECK(!config_has_bulk_in(other_alt.data(), other_alt.size(), 0x81));
    const auto alt_then_default = make_config({iface(0, 1), endpoint(0x82, 0x02, 512), iface(0, 0), endpoint(0x81, 0x02, 512)});
    CHECK(config_has_bulk_in(alt_then_default.data(), alt_then_default.size(), 0x81));
    const auto default_then_alt = make_config({iface(0, 0), endpoint(0x82, 0x02, 512), iface(0, 1), endpoint(0x81, 0x02, 512)});
    CHECK(!config_has_bulk_in(default_then_alt.data(), default_then_alt.size(), 0x81));

    /* malformed input fails closed */
    CHECK(!config_has_bulk_in(nullptr, 0, 0x81));
    CHECK(!config_has_bulk_in(good.data(), 8, 0x81));
    auto bad_type = good;
    bad_type[1] = 0x01;
    CHECK(!config_has_bulk_in(bad_type.data(), bad_type.size(), 0x81));
    auto short_header = good;
    short_header[0] = 8;
    CHECK(!config_has_bulk_in(short_header.data(), short_header.size(), 0x81));
    auto zero_len = make_config({iface(0, 0), std::vector<uint8_t>{0, 0x05}, endpoint(0x81, 0x02, 512)});
    CHECK(!config_has_bulk_in(zero_len.data(), zero_len.size(), 0x81));
    auto overrun = make_config({iface(0, 0), std::vector<uint8_t>{40, 0x05, 0x81, 0x02, 0, 2, 0}});
    CHECK(!config_has_bulk_in(overrun.data(), overrun.size(), 0x81));
    /* a declared total larger than the buffer is clamped to the buffer; one smaller hides later descriptors */
    auto truncated = good;
    truncated.resize(good.size() - 3);
    CHECK(!config_has_bulk_in(truncated.data(), truncated.size(), 0x81));
    auto declared_short = good;
    declared_short[2] = 9 + 9; /* only the interface descriptor is inside the declared length */
    CHECK(!config_has_bulk_in(declared_short.data(), declared_short.size(), 0x81));
}

/* ------------------------------------------------------------------ enumeration retry timing */

static void test_enum_retry_backoff()
{
    EnumRetry retry;
    CHECK(retry.current_wait_ms() == 10000);
    CHECK(!retry.counting());

    /* first sight of an empty bus only starts the clock */
    CHECK(!retry.no_device(1000));
    CHECK(retry.counting());
    CHECK(!retry.no_device(10999));
    CHECK(retry.no_device(11000));           /* 10 s elapsed: cycle */
    CHECK(retry.current_wait_ms() == 20000); /* the next wait doubled */
    CHECK(!retry.no_device(30999));
    CHECK(retry.no_device(31000));           /* 20 s later */
    CHECK(retry.current_wait_ms() == 40000);
    CHECK(retry.no_device(71000));           /* 40 s later */
    CHECK(retry.current_wait_ms() == 60000); /* capped */
    CHECK(!retry.no_device(130999));
    CHECK(retry.no_device(131000));          /* 60 s later */
    CHECK(retry.current_wait_ms() == 60000); /* stays at the cap */
    CHECK(retry.no_device(191000));

    /* after a power cycle the wait restarts from the cycle time */
    EnumRetry after_cycle;
    CHECK(!after_cycle.no_device(0));
    CHECK(after_cycle.no_device(10000));
    after_cycle.cycled(10250);
    CHECK(!after_cycle.no_device(30249));
    CHECK(after_cycle.no_device(30250));
}

static void test_enum_retry_reset_and_wrap()
{
    EnumRetry retry;
    CHECK(!retry.no_device(0));
    CHECK(retry.no_device(10000));
    CHECK(retry.current_wait_ms() == 20000);
    /* a device appears: everything starts over at the first wait */
    retry.device_present();
    CHECK(!retry.counting());
    CHECK(retry.current_wait_ms() == 10000);
    CHECK(!retry.no_device(50000));
    CHECK(!retry.no_device(59999));
    CHECK(retry.no_device(60000));

    /* a device that stays present never triggers a cycle, however long it runs */
    EnumRetry steady;
    for (uint32_t t = 0; t < 600000; t += 500) {
        steady.device_present();
        CHECK(!steady.counting());
    }

    /* the millisecond clock may wrap */
    EnumRetry wrap;
    const uint32_t near_end = 0xffffffffu - 4000;
    CHECK(!wrap.no_device(near_end));
    CHECK(!wrap.no_device(near_end + 9000));  /* wrapped past zero, 9 s elapsed */
    CHECK(wrap.no_device(near_end + 10000));  /* 10 s elapsed */
}

int main()
{
    test_ctrl_request_limits();
    test_ctrl_data_bytes();
    test_config_descriptor();
    test_enum_retry_backoff();
    test_enum_retry_reset_and_wrap();
    std::printf("RESULT usb_guard passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
