#include "rtl_acquisition.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>

static volatile uint64_t checksum;
int main() {
    constexpr size_t bytes=16384, iterations=20000;
    uint8_t input[bytes], output[bytes];
    for (size_t i=0;i<bytes;++i) input[i]=static_cast<uint8_t>(i);
    std::printf("host metadata_bytes=%zu sidecar64_bytes=%zu payload_bytes=%zu iterations=%zu\n",
                sizeof(esp_rtl_sdr_sample_meta_t),sizeof(rtl_acq::Sidecar<64>),bytes,iterations);
    void *(*volatile copy_block)(void *, const void *, size_t)=std::memcpy;
    for (int mode=-1;mode<3;++mode) {
        rtl_acq::Timeline timeline;rtl_acq::Cursor cursor;rtl_acq::Sidecar<64> sidecar;
        timeline.begin(1,100000000,2400000,2399999,0,ESP_RTL_SDR_ACQ_START);
        const unsigned stride=mode==1?64:mode==2?1:0;
        const auto begin=std::chrono::steady_clock::now();
        for (size_t i=0;i<iterations;++i) {
            input[i%bytes]=static_cast<uint8_t>(i);
            copy_block(output,input,bytes);
            checksum+=output[i%bytes];
            if (mode>=0) {
                auto meta=timeline.observe(bytes,static_cast<int64_t>(i));
                meta.quality=rtl_acq::quality(input,bytes,static_cast<uint16_t>(stride));
                sidecar.push(meta,bytes);
                size_t copied=0;sidecar.peek(bytes,meta,copied);
                meta=cursor.deliver(meta,static_cast<uint32_t>(copied/2));
                sidecar.consume(copied);
                checksum+=meta.sample_index+meta.quality.inspected_pairs;
            }
        }
        const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();
        std::printf("%s stride=%u ns_per_block=%.1f\n",mode<0?"copy_baseline":"copy_plus_metadata",stride,double(elapsed)/iterations);
    }
    return checksum==0;
}
