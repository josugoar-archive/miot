#include "esp_check.h"
#include "esp_log.h"
#include "gp2y0a41sk0f.h"

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/
// ADC1 Channels
#if CONFIG_IDF_TARGET_ESP32
#define EXAMPLE_ADC_CHAN ADC_CHANNEL_4
#else
#define EXAMPLE_ADC_CHAN ADC_CHANNEL_2
#endif

#define EXAMPLE_ADC_ATTEN ADC_ATTEN_DB_12

ESP_EVENT_DEFINE_BASE(GP2Y0A41SK0F_EVENT_BASE);

const static char *TAG = "GP2Y0A41SK0F";

static int adc_raw;
static int voltage;
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);
static void gp2y0a41sk0f_timer(void *arg);

esp_err_t gp2y0a41sk0f_init(struct gp2y0a41sk0f *gp2y0a41sk0f, esp_event_loop_handle_t event_loop)
{
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config, &gp2y0a41sk0f->adc_handle), TAG, "adc_oneshot_new_unit failed");

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(gp2y0a41sk0f->adc_handle, EXAMPLE_ADC_CHAN, &config), TAG, "adc_oneshot_config_channel failed");

    //-------------ADC1 Calibration Init---------------//
    gp2y0a41sk0f->adc_cali_handle = NULL;
    gp2y0a41sk0f->do_calibration = adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC_CHAN, EXAMPLE_ADC_ATTEN, &gp2y0a41sk0f->adc_cali_handle);

    esp_timer_create_args_t timer_conf = {
        .callback = gp2y0a41sk0f_timer,
        .arg = gp2y0a41sk0f,
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_conf, &gp2y0a41sk0f->timer), TAG, "esp_timer_create failed");

    gp2y0a41sk0f->event_loop = event_loop;

    return ESP_OK;
}

esp_err_t gp2y0a41sk0f_deinit(const struct gp2y0a41sk0f *gp2y0a41sk0f)
{
    // Tear Down
    ESP_RETURN_ON_ERROR(adc_oneshot_del_unit(gp2y0a41sk0f->adc_handle), TAG, "adc_oneshot_del_unit failed");
    if (gp2y0a41sk0f->do_calibration)
    {
        adc_calibration_deinit(gp2y0a41sk0f->adc_cali_handle);
    }

    ESP_RETURN_ON_ERROR(esp_timer_delete(gp2y0a41sk0f->timer), TAG, "esp_timer_delete failed");

    return ESP_OK;
}

esp_err_t gp2y0a41sk0f_start(const struct gp2y0a41sk0f *gp2y0a41sk0f)
{
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(gp2y0a41sk0f->timer, 1000000), TAG, "esp_timer_start_periodic failed");

    return ESP_OK;
}

esp_err_t gp2y0a41sk0f_stop(const struct gp2y0a41sk0f *gp2y0a41sk0f)
{
    ESP_RETURN_ON_ERROR(esp_timer_stop(gp2y0a41sk0f->timer), TAG, "esp_timer_stop failed");

    return ESP_OK;
}

esp_err_t gp2y0a41sk0f_read(const struct gp2y0a41sk0f *gp2y0a41sk0f, float *dist)
{
    ESP_RETURN_ON_ERROR(adc_oneshot_read(gp2y0a41sk0f->adc_handle, EXAMPLE_ADC_CHAN, &adc_raw), TAG, "adc_oneshot_read failed");
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC_CHAN, adc_raw);
    if (gp2y0a41sk0f->do_calibration)
    {
        ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(gp2y0a41sk0f->adc_cali_handle, adc_raw, &voltage), TAG, "adc_cali_raw_to_voltage failed");
        ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC_CHAN, voltage);
    }
    *dist = 11887.5 / (voltage - 0.03875);

    return ESP_OK;
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

static void gp2y0a41sk0f_timer(void *arg)
{
    struct gp2y0a41sk0f *gp2y0a41sk0f = (struct gp2y0a41sk0f *)arg;
    float dist;
    ESP_ERROR_CHECK(gp2y0a41sk0f_read(gp2y0a41sk0f, &dist));
    ESP_ERROR_CHECK(esp_event_post_to(gp2y0a41sk0f->event_loop, GP2Y0A41SK0F_EVENT_BASE, GP2Y0A41SK0F_EVENT_DATA, &dist, sizeof(dist), 0));
}
