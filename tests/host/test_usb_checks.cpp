/*
 * Host unit tests for the USB sanity checks in rtl_usb_checks.hpp
 * (control-transfer bounds, descriptor layout). No USB / FreeRTOS / IDF.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "rtl_usb_checks.hpp"

static int g_failed = 0;
static int g_passed = 0;

#define EXPECT_TRUE(cond)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                            \
            g_failed++;                                                                            \
        } else {                                                                                   \
            g_passed++;                                                                            \
        }                                                                                          \
    } while (0)

#define EXPECT_EQ_U(a, b)                                                                          \
    do {                                                                                           \
        const auto _a = (a);                                                                       \
        const auto _b = (b);                                                                       \
        if (_a != _b) {                                                                            \
            std::printf("FAIL %s:%d: %s (%u) != %s (%u)\n", __FILE__, __LINE__, #a,                \
                        (unsigned)_a, #b, (unsigned)_b);                                           \
            g_failed++;                                                                            \
        } else {                                                                                   \
            g_passed++;                                                                            \
        }                                                                                          \
    } while (0)

/* Same size as the driver's control buffer: 64 data bytes + setup packet. */
static constexpr size_t kBuf = 64 + kRtlUsbSetupBytes;

static void test_ctrl_length_bounds(void)
{
    EXPECT_TRUE(rtl_ctrl_request_ok(0x40, 0x0610, 0, kBuf));
    EXPECT_TRUE(rtl_ctrl_request_ok(0x40, 0x0110, 64, kBuf));
    EXPECT_TRUE(!rtl_ctrl_request_ok(0x40, 0x0110, 65, kBuf));
    EXPECT_TRUE(!rtl_ctrl_request_ok(0xc0, 0x0100, 65, kBuf));
    EXPECT_TRUE(!rtl_ctrl_request_ok(0xc0, 0x0100, 0xffff, kBuf));
    /* Buffer smaller than a setup packet: nothing fits. */
    EXPECT_TRUE(!rtl_ctrl_request_ok(0x40, 0x0610, 0, 4));
}

static void test_ctrl_i2c_read_limit(void)
{
    /* I2C passthrough (block 6) reads: up to 16 bytes. */
    EXPECT_TRUE(rtl_ctrl_request_ok(0xc0, 0x0600, 1, kBuf));
    EXPECT_TRUE(rtl_ctrl_request_ok(0xc0, 0x0600, 16, kBuf));
    EXPECT_TRUE(!rtl_ctrl_request_ok(0xc0, 0x0600, 17, kBuf));
    EXPECT_TRUE(!rtl_ctrl_request_ok(0xc0, 0x0600, 32, kBuf));
    /* Writes to the same block, and reads from other blocks, are not limited to 16. */
    EXPECT_TRUE(rtl_ctrl_request_ok(0x40, 0x0610, 32, kBuf));
    EXPECT_TRUE(rtl_ctrl_request_ok(0xc0, 0x0100, 32, kBuf));
    EXPECT_TRUE(rtl_ctrl_request_ok(0xc0, 0x0200, 32, kBuf));
    /* Hub class requests carry a port number in wIndex. */
    EXPECT_TRUE(rtl_ctrl_request_ok(0xa3, 0x0001, 4, kBuf));
}

static void test_ctrl_data_stage_bytes(void)
{
    EXPECT_EQ_U(rtl_ctrl_data_stage_bytes(-1), 0u);
    EXPECT_EQ_U(rtl_ctrl_data_stage_bytes(0), 0u);
    EXPECT_EQ_U(rtl_ctrl_data_stage_bytes(8), 0u);
    EXPECT_EQ_U(rtl_ctrl_data_stage_bytes(9), 1u);
    EXPECT_EQ_U(rtl_ctrl_data_stage_bytes(8 + 64), 64u);
    EXPECT_EQ_U(rtl_ctrl_data_stage_bytes(8 + 0x20000), 0xffffu);
}

/* Builds a configuration descriptor: one interface (number, alt) with the given endpoints. */
struct Ep {
    uint8_t addr;
    uint8_t attrs;
    uint16_t mps;
};

static std::vector<uint8_t> config_with(const std::vector<std::pair<uint8_t, std::vector<Ep>>> &ifs)
{
    std::vector<uint8_t> d = {9, 0x02, 0, 0, static_cast<uint8_t>(ifs.size()), 1, 0, 0x80, 250};
    for (const auto &intf : ifs) {
        const uint8_t num = static_cast<uint8_t>(intf.first >> 4);
        const uint8_t alt = static_cast<uint8_t>(intf.first & 0x0f);
        const uint8_t id[] = {9, 0x04, num, alt, static_cast<uint8_t>(intf.second.size()),
                              0xff, 0xff, 0xff, 0};
        d.insert(d.end(), id, id + sizeof(id));
        for (const Ep &e : intf.second) {
            const uint8_t ed[] = {7,
                                  0x05,
                                  e.addr,
                                  e.attrs,
                                  static_cast<uint8_t>(e.mps & 0xff),
                                  static_cast<uint8_t>(e.mps >> 8),
                                  0};
            d.insert(d.end(), ed, ed + sizeof(ed));
        }
    }
    d[2] = static_cast<uint8_t>(d.size() & 0xff);
    d[3] = static_cast<uint8_t>(d.size() >> 8);
    return d;
}

static bool layout_ok(const std::vector<uint8_t> &d)
{
    return rtl_config_desc_has_bulk_in(d.data(), d.size(), 0x81);
}

static void test_layout_rtl2832u(void)
{
    /* RTL2832U: interface 0 with bulk IN 0x81 (512 bytes at high speed), interface 1 vendor. */
    const auto hs = config_with({{0x00, {{0x81, 0x02, 512}}}, {0x10, {}}});
    EXPECT_TRUE(layout_ok(hs));
    const auto fs = config_with({{0x00, {{0x81, 0x02, 64}}}});
    EXPECT_TRUE(layout_ok(fs));
    /* Endpoint listed after another endpoint on the same interface. */
    const auto two = config_with({{0x00, {{0x02, 0x02, 512}, {0x81, 0x02, 512}}}});
    EXPECT_TRUE(layout_ok(two));
}

static void test_layout_rejects(void)
{
    /* No endpoints at all. */
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {}}})));
    /* 0x81 is interrupt, isochronous, or OUT 0x01 instead. */
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {{0x81, 0x03, 64}}}})));
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {{0x81, 0x01, 512}}}})));
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {{0x01, 0x02, 512}}}})));
    /* Bulk IN 0x81 only on interface 1, or only on interface 0 alternate setting 1. */
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {}}, {0x10, {{0x81, 0x02, 512}}}})));
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {}}, {0x01, {{0x81, 0x02, 512}}}})));
    /* Packet size out of range (bits 11..12 are high-bandwidth multipliers, not size). */
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {{0x81, 0x02, 0}}}})));
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {{0x81, 0x02, 4}}}})));
    EXPECT_TRUE(!layout_ok(config_with({{0x00, {{0x81, 0x02, 1024}}}})));
    EXPECT_TRUE(layout_ok(config_with({{0x00, {{0x81, 0x02, 0x1800 | 512}}}})));
}

static void test_layout_malformed(void)
{
    const auto good = config_with({{0x00, {{0x81, 0x02, 512}}}});
    EXPECT_TRUE(!rtl_config_desc_has_bulk_in(nullptr, 0, 0x81));
    EXPECT_TRUE(!rtl_config_desc_has_bulk_in(good.data(), 8, 0x81));
    /* Truncated before the endpoint descriptor. */
    EXPECT_TRUE(!rtl_config_desc_has_bulk_in(good.data(), good.size() - 7, 0x81));
    /* wTotalLength claims less than the buffer: only wTotalLength is walked. */
    auto short_total = good;
    short_total[2] = 18;
    EXPECT_TRUE(!layout_ok(short_total));
    /* Not a configuration descriptor. */
    auto wrong_type = good;
    wrong_type[1] = 0x01;
    EXPECT_TRUE(!layout_ok(wrong_type));
    /* A zero bLength would loop forever; an overlong one runs past the end. */
    auto zero_len = good;
    zero_len[9] = 0;
    EXPECT_TRUE(!layout_ok(zero_len));
    auto long_len = good;
    long_len[18] = 200;
    EXPECT_TRUE(!layout_ok(long_len));
}

int main(void)
{
    test_ctrl_length_bounds();
    test_ctrl_i2c_read_limit();
    test_ctrl_data_stage_bytes();
    test_layout_rtl2832u();
    test_layout_rejects();
    test_layout_malformed();

    std::printf("usb_checks tests: passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
