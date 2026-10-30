#include "esp_rtl_sdr_acquisition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>

#define RECEIVERS CONFIG_ESP_RTL_SDR_PROBE_RECEIVERS
static portMUX_TYPE s_mux=portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_data[RECEIVERS][16384];
static struct receiver {
    esp_rtl_sdr_handle_t handle;
    unsigned index;
    bool running,done;
    uint64_t bytes,blocks,errors,known_lost,unknown_markers,last_unknown,receiver_id,epoch,end;
    esp_rtl_sdr_iq_quality_t quality;
    uint32_t frequency,rate;
} s_receiver[RECEIVERS];
static unsigned s_failures;

static void reader(void *arg)
{
    struct receiver *r=arg;
    for (;;) {
        portENTER_CRITICAL(&s_mux);bool running=r->running;portEXIT_CRITICAL(&s_mux);
        if (!running) break;
        esp_rtl_sdr_sample_meta_t meta;
        esp_rtl_sdr_sample_meta_default(&meta);
        size_t bytes=0;
        esp_err_t err=esp_rtl_sdr_read_ex(r->handle,s_data[r->index],sizeof(s_data[0]),20,&bytes,&meta);
        if (err!=ESP_OK) {
            if (err!=ESP_RTL_SDR_ERR_TIMEOUT) {
                portENTER_CRITICAL(&s_mux);++r->errors;portEXIT_CRITICAL(&s_mux);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            continue;
        }
        portENTER_CRITICAL(&s_mux);
        bool bad=meta.sample_count!=bytes/2 || !meta.receiver_id || !meta.epoch || !meta.quality.inspected_pairs ||
            meta.sample_index<meta.block_sample_index ||
            meta.sample_index+meta.sample_count>meta.block_sample_index+meta.block_sample_count ||
            meta.device_id!=r->index || meta.estimated_rate_millisps!=0;
        if (r->receiver_id && r->receiver_id!=meta.receiver_id) bad=true;
        if (r->epoch==meta.epoch &&
            (meta.sample_index<r->end || meta.known_lost_samples!=meta.sample_index-r->end ||
             meta.center_frequency_hz!=r->frequency || meta.programmed_rate_sps!=r->rate)) bad=true;
        if (meta.epoch<r->epoch) bad=true;
        const uint64_t previous_unknown=r->epoch==meta.epoch?r->last_unknown:0;
        if (meta.unknown_gap_count>previous_unknown) {
            if (!(meta.flags & ESP_RTL_SDR_ACQ_UNKNOWN_LOSS)) bad=true;
            r->unknown_markers+=meta.unknown_gap_count-previous_unknown;
        }
        r->last_unknown=meta.unknown_gap_count;r->quality=meta.quality;
        r->errors+=bad;r->bytes+=bytes;++r->blocks;r->known_lost+=meta.known_lost_samples;
        r->receiver_id=meta.receiver_id;r->epoch=meta.epoch;r->end=meta.sample_index+meta.sample_count;
        r->frequency=meta.center_frequency_hz;r->rate=meta.programmed_rate_sps;
        portEXIT_CRITICAL(&s_mux);
    }
    portENTER_CRITICAL(&s_mux);r->done=true;portEXIT_CRITICAL(&s_mux);
    vTaskDelete(NULL);
}

static bool start_reader(struct receiver *r)
{
    portENTER_CRITICAL(&s_mux);r->running=true;r->done=false;portEXIT_CRITICAL(&s_mux);
    if (xTaskCreatePinnedToCore(reader,"acq_reader",4096,r,5,NULL,portNUM_PROCESSORS>1?1:0)==pdPASS) return true;
    portENTER_CRITICAL(&s_mux);r->running=false;r->done=true;portEXIT_CRITICAL(&s_mux);
    return false;
}

static bool stop_reader(struct receiver *r)
{
    portENTER_CRITICAL(&s_mux);r->running=false;portEXIT_CRITICAL(&s_mux);
    for (int i=0;i<100;++i) {
        portENTER_CRITICAL(&s_mux);bool done=r->done;portEXIT_CRITICAL(&s_mux);
        if (done) return true;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

static void report(unsigned elapsed)
{
    for (unsigned i=0;i<RECEIVERS;++i) {
        struct receiver *r=&s_receiver[i];
        portENTER_CRITICAL(&s_mux);
        const uint64_t id=r->receiver_id,epoch=r->epoch,bytes=r->bytes,blocks=r->blocks,errors=r->errors,lost=r->known_lost,unknown=r->unknown_markers;
        const esp_rtl_sdr_iq_quality_t quality=r->quality;
        portEXIT_CRITICAL(&s_mux);
        esp_rtl_sdr_stream_stats_t stats;
        esp_rtl_sdr_stream_stats_default(&stats);
        if (r->handle) (void)esp_rtl_sdr_get_stream_stats(r->handle,&stats);
        printf("ACQ STATUS ms=%u receiver=%u id=%llu epoch=%llu delivered=%llu blocks=%llu errors=%llu known_lost=%llu unknown_markers=%llu received=%llu usb_errors=%u ring_lost=%llu\n",
            elapsed,i,(unsigned long long)id,(unsigned long long)epoch,(unsigned long long)bytes,
            (unsigned long long)blocks,(unsigned long long)errors,(unsigned long long)lost,(unsigned long long)unknown,
            (unsigned long long)stats.bytes_received,(unsigned)stats.usb_transfer_errors,(unsigned long long)stats.ring_overrun_bytes);
        printf("ACQ QUALITY receiver=%u inspected_pairs=%u stride=%u rails=%u mean_i_milli=%u mean_q_milli=%u rms_milli=%u scope=last_usb_block\n",
            i,(unsigned)quality.inspected_pairs,(unsigned)quality.stride_pairs,(unsigned)quality.rail_components,
            (unsigned)quality.mean_i_milli,(unsigned)quality.mean_q_milli,(unsigned)quality.rms_milli);
    }
}

void app_main(void)
{
    printf("ACQ PROBE receivers=%u duration_ms=%u board_power=external\n",RECEIVERS,CONFIG_ESP_RTL_SDR_PROBE_DURATION_MS);
    bool ready=true;
    for (unsigned i=0;i<RECEIVERS;++i) {
        struct receiver *r=&s_receiver[i];r->index=i;r->done=true;
        esp_rtl_sdr_config_t config;esp_rtl_sdr_config_default(&config);
        config.delivery_mode=ESP_RTL_SDR_DELIVERY_READ;
        config.transfer_count=2;config.pull_ring_bytes=65536;
        esp_err_t err=esp_rtl_sdr_install(&config,&r->handle);
        if (err==ESP_OK) {
            esp_rtl_sdr_acquisition_config_t acquisition;esp_rtl_sdr_acquisition_config_default(&acquisition);
            acquisition.enabled=true;acquisition.quality_stride_pairs=64;
            err=esp_rtl_sdr_configure_acquisition(r->handle,&acquisition);
        }
        printf("ACQ INSTALL receiver=%u result=%d\n",i,(int)err);
        if (err!=ESP_OK) {ready=false;++s_failures;}
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
    if (ready) {
        for (unsigned i=0;i<RECEIVERS;++i) {
            struct receiver *r=&s_receiver[i];
            const uint32_t rate=i==0?960000:i==1?2400000:2048000;
            esp_err_t err=esp_rtl_sdr_start_hz(r->handle,100100000+i*1000000,rate);
            esp_rtl_sdr_identity_t id;esp_rtl_sdr_identity_default(&id);
            (void)esp_rtl_sdr_get_identity(r->handle,&id);
            printf("ACQ START receiver=%u result=%d requested=%u path=%s profile=%s manufacturer=%s product=%s\n",i,(int)err,(unsigned)rate,id.usb_path,esp_rtl_sdr_profile_to_name(id.profile),id.manufacturer,id.product);
            if (err!=ESP_OK || !start_reader(r)) {ready=false;++s_failures;}
        }
    }
    const int64_t start=esp_timer_get_time();
    unsigned last_report=0;bool retuned=false,restarted=false,released=false;
    while (ready) {
        const unsigned elapsed=(unsigned)((esp_timer_get_time()-start)/1000);
        if (elapsed>=CONFIG_ESP_RTL_SDR_PROBE_DURATION_MS) break;
        if (!retuned && elapsed>=CONFIG_ESP_RTL_SDR_PROBE_DURATION_MS/4) {
            esp_err_t err=esp_rtl_sdr_retune_hz(s_receiver[0].handle,99100000);
            printf("ACQ RETUNE receiver=0 result=%d\n",(int)err);s_failures+=err!=ESP_OK;retuned=true;
        }
        if (!restarted && elapsed>=CONFIG_ESP_RTL_SDR_PROBE_DURATION_MS/2) {
            struct receiver *r=&s_receiver[0];
            if (!stop_reader(r)) {++s_failures;break;}
            esp_err_t err=esp_rtl_sdr_stop(r->handle,1000);
            if (err==ESP_OK) err=esp_rtl_sdr_start_hz(r->handle,101100000,1024000);
            if (err==ESP_OK && !start_reader(r)) err=ESP_ERR_NO_MEM;
            printf("ACQ RESTART receiver=0 result=%d\n",(int)err);s_failures+=err!=ESP_OK;restarted=true;
        }
        if (!released && elapsed>=CONFIG_ESP_RTL_SDR_PROBE_DURATION_MS*3/4) {
            struct receiver *r=&s_receiver[0];
            if (!stop_reader(r)) {++s_failures;break;}
            esp_err_t err=esp_rtl_sdr_uninstall(r->handle);
            if (err==ESP_OK) r->handle=NULL;
            printf("ACQ RELEASE receiver=0 result=%d others=%u\n",(int)err,RECEIVERS-1);s_failures+=err!=ESP_OK;released=true;
            if (RECEIVERS==1) break;
        }
        if (elapsed-last_report>=10000) {report(elapsed);last_report=elapsed;}
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    report((unsigned)((esp_timer_get_time()-start)/1000));
    for (unsigned i=0;i<RECEIVERS;++i) {
        struct receiver *r=&s_receiver[i];
        if (r->handle) {
            if (!stop_reader(r)) {++s_failures;continue;}
            esp_err_t err=esp_rtl_sdr_uninstall(r->handle);
            printf("ACQ CLEANUP receiver=%u result=%d\n",i,(int)err);
            if (err==ESP_OK) r->handle=NULL;else ++s_failures;
        }
        portENTER_CRITICAL(&s_mux);s_failures+=(r->errors!=0 || r->blocks==0);portEXIT_CRITICAL(&s_mux);
    }
    for (unsigned i=0;i<RECEIVERS;++i) for (unsigned j=i+1;j<RECEIVERS;++j)
        if (s_receiver[i].receiver_id==s_receiver[j].receiver_id) ++s_failures;
    printf("ACQ PROBE OVERALL %s failures=%u\n",s_failures?"FAIL":"PASS",s_failures);
}
