#include "rtl_acquisition.hpp"
#include <cstdio>
#include <limits>
int main() {
    int failures=0;
    auto check=[&](bool ok){if(!ok){++failures;std::puts("FAIL acquisition contract");}};
    rtl_acq::Timeline a, b;
    check(a.begin(7, 100000000, 2400000, 2399999, 0, ESP_RTL_SDR_ACQ_START));
    auto x=a.observe(8, 100); auto y=a.observe(8, 120);
    check(x.epoch==1 && x.sample_index==0 && x.sample_count==4 && y.sample_index==4);
    check(x.receiver_id==7 && x.requested_rate_sps==2400000 && x.programmed_rate_sps==2399999);
    rtl_acq::Cursor cursor;
    auto first=cursor.deliver(x,4); auto gap=cursor.deliver(a.observe(8,140),4);
    check(first.flags & ESP_RTL_SDR_ACQ_START);
    check(gap.known_lost_samples==4 && (gap.flags & ESP_RTL_SDR_ACQ_KNOWN_LOSS));
    a.interrupt(ESP_RTL_SDR_ACQ_USB_ERROR);
    auto unknown=cursor.deliver(a.observe(8,160),4);
    check((unknown.flags & ESP_RTL_SDR_ACQ_UNKNOWN_LOSS) && unknown.known_lost_samples==0);
    auto partial=cursor.deliver(a.observe(8,180),2);
    check(partial.sample_count==2);
    check(!(partial.flags & (ESP_RTL_SDR_ACQ_UNKNOWN_LOSS | ESP_RTL_SDR_ACQ_USB_ERROR)));
    check(a.begin(7,137100000,960000,960000,0,ESP_RTL_SDR_ACQ_RETUNE));
    auto retuned=a.observe(8,200);
    check(retuned.epoch==2 && retuned.sample_index==0 && x.center_frequency_hz==100000000);
    check(cursor.deliver(retuned,4).flags & ESP_RTL_SDR_ACQ_RETUNE);
    check(b.begin(8,162000000,960000,960000,0,ESP_RTL_SDR_ACQ_START));
    check(b.observe(8,210).sample_index==0 && b.epoch()==1);
    rtl_acq::Sidecar<2> q;
    check(q.push(x,8) && q.push(retuned,8) && !q.push(y,8));
    esp_rtl_sdr_sample_meta_t m{}; size_t n=0;
    check(q.peek(2,m,n) && n==2 && m.epoch==1 && m.sample_index==0);
    q.consume(2); check(q.peek(20,m,n) && n==6 && m.sample_index==1);
    q.consume(6); check(q.peek(20,m,n) && n==8 && m.epoch==2);
    q.consume(8); check(!q.peek(2,m,n));
    check(a.begin(7,0,960000,960000,0,ESP_RTL_SDR_ACQ_CONTROL_FAILED));
    check(a.observe(4,220).center_frequency_hz==0);
    uint64_t maximum=std::numeric_limits<uint64_t>::max();
    check(!rtl_acq::advance(maximum,1) && maximum==std::numeric_limits<uint64_t>::max());
    const uint8_t rails[]={0,255,0,255,0,255,0,255};
    auto quality=rtl_acq::quality(rails,sizeof(rails),1);
    check(quality.inspected_pairs==4 && quality.rail_components==8 && quality.mean_i_milli==0 && quality.mean_q_milli==255000);
    check(quality.mean_i_power_x4==65025 && quality.mean_q_power_x4==65025 && quality.rms_milli==180312);
    check(rtl_acq::quality(rails,sizeof(rails),2).inspected_pairs==2);
    check(rtl_acq::quality(rails,sizeof(rails),0).inspected_pairs==0);
    esp_rtl_sdr_acquisition_config_t config{};
    auto clock=retuned;
    rtl_acq::clock_estimate(clock,config);check(clock.estimated_rate_millisps==0);
    config.clock_estimate_valid=true;config.sample_clock_error_ppb=1000;config.clock_uncertainty_ppb=20;
    rtl_acq::clock_estimate(clock,config);
    check(clock.estimated_rate_millisps==960000960 && clock.clock_uncertainty_ppb==20);
    rtl_acq::Timeline malformed;
    check(malformed.begin(9,100000000,960000,960000,0,ESP_RTL_SDR_ACQ_START));
    check(malformed.observe(3,1).sample_count==0);
    check(malformed.observe(4,2).sample_count==0);
    check(malformed.begin(9,100000000,960000,960000,0,ESP_RTL_SDR_ACQ_START));
    check(malformed.observe(4,3).sample_count==2);
    std::printf("acquisition failures=%d\n",failures); return failures?1:0;
}
