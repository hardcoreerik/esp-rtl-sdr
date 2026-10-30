#include "esp_rtl_sdr_acquisition.h"
int main(void) {
    esp_rtl_sdr_acquisition_config_t config;
    esp_rtl_sdr_sample_meta_t metadata;
    esp_rtl_sdr_acquisition_config_default(&config);
    esp_rtl_sdr_sample_meta_default(&metadata);
    return config.struct_size != sizeof(config) || config.enabled ||
           metadata.struct_size != sizeof(metadata) || metadata.estimated_rate_millisps != 0;
}
