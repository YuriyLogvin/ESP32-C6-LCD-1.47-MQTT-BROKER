#include <drivers\display\st7789\lv_st7789.h>
#include "esp_log.h"
#include "esp_check.h"
#include "SpiMipiLvglDisplayDriver.h"

static const char *TAG = "smld";

void lv_lcd_send_cmd_cb(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t * param, size_t param_size);
void lv_lcd_send_color_cb(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size);


static SpiMipiLvglDisplayDriver *_ActiveSpiMipiLvglDisplayDriver = NULL;

SpiMipiLvglDisplayDriver::SpiMipiLvglDisplayDriver(spi_host_device_t hostId, gpio_num_t gpioCs, gpio_num_t gpioDc, gpio_num_t gpioRst, uint32_t clockSpeedHz, gpio_num_t gpioBacklight)
{
    _HostId = hostId;
    _GpioCs = gpioCs;
    _GpioDc = gpioDc;
    _GpioRst = gpioRst;
    _GpioBackLight = gpioBacklight;
    _SpiHandle = NULL;

    ESP_LOGI(TAG, "->SpiMipiLvglDisplayDriver %x:%x:%x", hostId, lv_lcd_send_cmd_cb, lv_lcd_send_color_cb);

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = (int)clockSpeedHz,
        .spics_io_num = gpioCs,
        .queue_size = 4, 
    };

    if (gpioRst != GPIO_NUM_NC)
    {
        gpio_reset_pin(gpioRst);
        gpio_set_direction(gpioRst, GPIO_MODE_OUTPUT);
        gpio_set_level(gpioRst, 1);
    }

    if (gpioCs != GPIO_NUM_NC)
    {
        gpio_reset_pin(gpioCs);
        gpio_set_direction(gpioCs, GPIO_MODE_OUTPUT);
        gpio_set_level(_GpioCs, 1);
    }

    if (gpioDc != GPIO_NUM_NC)
    {
        gpio_reset_pin(gpioDc);
        gpio_set_direction(gpioDc, GPIO_MODE_OUTPUT);
        gpio_set_level(_GpioDc, 1);
    }

    auto ret = spi_bus_add_device(hostId, &devcfg, &_SpiHandle);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "spi_bus_add_device failed with rc=0x%x", ret);
        return;
    }

    if (_GpioRst != GPIO_NUM_NC)
    {
        //gpio_reset_pin(_GpioRst);
        gpio_set_level(_GpioRst, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(_GpioRst, 0);
        vTaskDelay(pdMS_TO_TICKS(10));  
        gpio_set_level(_GpioRst, 1);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    lv_init();

    lv_delay_set_cb([](uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); });    

    lv_tick_set_cb([]() { return (uint32_t)(esp_timer_get_time() / 1000); });
    lv_tick_inc(0); 
    
    _ActiveSpiMipiLvglDisplayDriver = this;

//TODO:  LV_USE_ST7735 | LV_USE_ST7789 | LV_USE_ST7796 | LV_USE_ILI9341   
#ifdef LV_USE_ST7789
    ESP_LOGI(TAG, "->lv_st7789_create");
    _Display = lv_st7789_create(ST7789_LCD_H_RES, ST7789_LCD_V_RES, LV_LCD_FLAG_NONE, lv_lcd_send_cmd_cb, lv_lcd_send_color_cb);
    #define hRes ST7789_LCD_H_RES
    #define vRes ST7789_LCD_V_RES
#elifdef LV_USE_ST7735
    ESP_LOGI(TAG, "->lv_st7735_create");
    _Display = lv_st7735_create(ST7735_LCD_H_RES, ST7735_LCD_V_RES, LV_LCD_FLAG_NONE, lv_lcd_send_cmd_cb, lv_lcd_send_color_cb);    
    #define hRes ST7735_LCD_H_RES
    #define vRes ST7735_LCD_V_RES
#elifdef LV_USE_ST7796
    ESP_LOGI(TAG, "->lv_st7796_create");
    _Display = lv_st7796_create(ST7796_LCD_H_RES, ST7796_LCD_V_RES, LV_LCD_FLAG_NONE, lv_lcd_send_cmd_cb, lv_lcd_send_color_cb);
    #define hRes ST7796_LCD_H_RES
    #define vRes ST7796_LCD_V_RES
#elifdef LV_USE_ILI9341
    ESP_LOGI(TAG, "->lv_ili9341_create");
    _Display = lv_ili9341_create(ST7789_LCD_H_RES, ST7789_LCD_V_RES, LV_LCD_FLAG_NONE, lv_lcd_send_cmd_cb, lv_lcd_send_color_cb);
    #define hRes ST9341_LCD_H_RES
    #define vRes ST9341_LCD_V_RES
#else
    ESP_LOGE(TAG, "No LCD driver defined. Please enable LV_USE_ST7789 in lv_conf.h");
    _Display = NULL;
    return;
#endif // LV_USE_ST7789

    lv_lcd_generic_mipi_set_invert(_Display, true);
    lv_display_set_rotation(_Display, LV_DISPLAY_ROTATION_0);

    static uint8_t buf[hRes * vRes / 10 * 2]; /* x2 because of 16-bit color depth */
    lv_display_set_buffers(_Display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);    

    lv_display_set_user_data(_Display, this);

    _BackLightInit();
    
    SetBackLight(50);

}

void SpiMipiLvglDisplayDriver::_BackLightInit()
{
    ESP_LOGI(TAG, "backlight init GPIO %d", _GpioBackLight);
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << _GpioBackLight,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LS_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_HS_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    _LedcChannel.channel    = LEDC_HS_CH0_CHANNEL;
    _LedcChannel.gpio_num   = _GpioBackLight;
    _LedcChannel.speed_mode = LEDC_LS_MODE;
    _LedcChannel.timer_sel  = LEDC_HS_TIMER;
    _LedcChannel.duty       = 0;
    _LedcChannel.intr_type = LEDC_INTR_DISABLE;
    _LedcChannel.hpoint = 0;
    _LedcChannel.flags.output_invert = 0;
    _LedcChannel.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    ledc_channel_config(&_LedcChannel);
    ledc_fade_func_install(0);
}

void SpiMipiLvglDisplayDriver::SetBackLight(uint8_t value)
{   
    if(value > 100) 
        value = 100;
    uint16_t duty = LEDC_MAX_Duty-(81*(100-value));
    if(value == 0) 
        duty = 0;
    ledc_set_duty(_LedcChannel.speed_mode, _LedcChannel.channel, duty);
    ledc_update_duty(_LedcChannel.speed_mode, _LedcChannel.channel);
}

SpiMipiLvglDisplayDriver::~SpiMipiLvglDisplayDriver()
{
    if (_Display)
    {
        //lv_st7789_delete(_Display);
        _Display = NULL;
    };
     
    if (_SpiHandle)
    {
        spi_bus_remove_device(_SpiHandle);
        _SpiHandle = NULL;
    }
}

lv_display_t* SpiMipiLvglDisplayDriver::operator &()
{
    return _Display;
}

void SpiMipiLvglDisplayDriver::_lv_lcd_send_cmd_cb(const uint8_t *cmd, size_t cmd_size, const uint8_t *param, size_t param_size)
{
    spi_transaction_t trConfig = 
    {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = 0,
        .rxlength = 0,
        .user = 0,
        .tx_buffer = 0,
        .rx_buffer = 0
    };

    if (cmd_size)
    {
        trConfig.length = cmd_size*8;
        trConfig.tx_buffer = (void*)cmd;

        if (_GpioDc != GPIO_NUM_NC)
            gpio_set_level(_GpioDc, 0);

        auto ret = spi_device_transmit(_SpiHandle, &trConfig);
        if (ret != ESP_OK) {
            ESP_LOGD(TAG, "spi_device_transmit failed with rc=0x%x", ret);
        }

        if (_GpioDc != GPIO_NUM_NC)
            gpio_set_level(_GpioDc, 1);
    }

    if (param_size)
    {
        trConfig.length = param_size*8;
        trConfig.tx_buffer = (void*)param;

        auto ret = spi_device_transmit(_SpiHandle, &trConfig);
        if (ret != ESP_OK) {    
            ESP_LOGD(TAG, "spi_device_transmit failed with rc=0x%x", ret);
        }       
    }

}

void SpiMipiLvglDisplayDriver::_lv_lcd_send_color_cb(const uint8_t *cmd, size_t cmd_size, uint8_t *param, size_t param_size)
{
    _lv_lcd_send_cmd_cb(cmd, cmd_size, param, param_size);
    //vTaskDelay(pdMS_TO_TICKS(10));
    lv_display_flush_ready(_Display);
}

void lv_lcd_send_cmd_cb(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t * param, size_t param_size)
{
    //ESP_LOGI(TAG, "%s:%i:%i", __FUNCTION__, cmd_size, param_size);
    SpiMipiLvglDisplayDriver* drv = (SpiMipiLvglDisplayDriver*)lv_display_get_user_data(disp);
    if (drv == NULL)
        drv = _ActiveSpiMipiLvglDisplayDriver;
    drv->_lv_lcd_send_cmd_cb(cmd, cmd_size, param, param_size);
}

void lv_lcd_send_color_cb(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size)
{
    //ESP_LOGI(TAG, "%s:%i:%i", __FUNCTION__, cmd_size, param_size);
    SpiMipiLvglDisplayDriver* drv = (SpiMipiLvglDisplayDriver*)lv_display_get_user_data(disp);
    if (drv == NULL)
        drv = _ActiveSpiMipiLvglDisplayDriver;
    drv->_lv_lcd_send_color_cb(cmd, cmd_size, param, param_size);
}

