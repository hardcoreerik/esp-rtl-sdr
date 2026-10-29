#include "rtl_pull_alloc.hpp"
#include <cstdio>
#include <vector>
struct Heap {
    size_t external = 0, budget = 0, limit = 0;
    std::vector<size_t> internal_requests;
    unsigned char byte{};
    unsigned char *psram(size_t n) { return n <= external ? &byte : nullptr; }
    size_t internal_budget() { return budget; }
    unsigned char *internal(size_t n) { internal_requests.push_back(n); return n <= limit ? &byte : nullptr; }
};
int main() {
    int fails = 0; size_t actual = 0;
    auto check = [&](bool ok) { if (!ok) { ++fails; std::puts("FAIL pull allocation"); } };
    Heap psram; psram.external = 512*1024;
    check(rtl_usb::allocate_pull(psram, 384*1024, true, actual) != nullptr && actual == 384*1024);
    check(psram.internal_requests.empty());
    Heap internal; internal.budget = 128*1024; internal.limit = 128*1024;
    check(rtl_usb::allocate_pull(internal, 384*1024, true, actual) != nullptr && actual == 128*1024);
    Heap fragmentation; fragmentation.budget = 256*1024; fragmentation.limit = 64*1024;
    check(rtl_usb::allocate_pull(fragmentation, 384*1024, true, actual) != nullptr && actual == 64*1024);
    Heap near_floor; near_floor.budget = 96*1024; near_floor.limit = 64*1024;
    check(rtl_usb::allocate_pull(near_floor, 384*1024, true, actual) != nullptr && actual == 64*1024);
    Heap low; low.budget = 32*1024; low.limit = 32*1024;
    check(rtl_usb::allocate_pull(low, 384*1024, true, actual) == nullptr && low.internal_requests.empty());
    Heap explicit_size; explicit_size.budget=64*1024; explicit_size.limit=64*1024;
    check(rtl_usb::allocate_pull(explicit_size, 384*1024, false, actual) == nullptr);
    check(explicit_size.internal_requests == std::vector<size_t>({384*1024}));
    check(rtl_usb::allocate_pull(explicit_size, 1024, false, actual) != nullptr && actual == 1024);
    std::printf("pull allocation failures=%d\n", fails); return fails ? 1 : 0;
}
