#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_timer.h"

ESP_EVENT_DECLARE_BASE(GP2Y0A41SK0F_EVENT_BASE);

enum
{
    GP2Y0A41SK0F_EVENT_DATA
};

struct gp2y0a41sk0f
{
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
    bool do_calibration;
    esp_timer_handle_t timer;
    esp_event_loop_handle_t event_loop;
};

esp_err_t gp2y0a41sk0f_init(struct gp2y0a41sk0f *gp2y0a41sk0f, esp_event_loop_handle_t event_loop);

esp_err_t gp2y0a41sk0f_deinit(const struct gp2y0a41sk0f *gp2y0a41sk0f);

esp_err_t gp2y0a41sk0f_start(const struct gp2y0a41sk0f *gp2y0a41sk0f);

esp_err_t gp2y0a41sk0f_stop(const struct gp2y0a41sk0f *gp2y0a41sk0f);

esp_err_t gp2y0a41sk0f_read(const struct gp2y0a41sk0f *gp2y0a41sk0f, float *dist);
