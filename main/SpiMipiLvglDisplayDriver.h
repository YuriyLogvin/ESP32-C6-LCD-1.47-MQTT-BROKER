#pragma once
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

// The pixel number in horizontal and vertical
#define ST7789_LCD_H_RES              240
#define ST7789_LCD_V_RES              320

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_TEST_DUTY         (4000)
#define LEDC_ResolutionRatio   LEDC_TIMER_13_BIT
#define LEDC_MAX_Duty          ((1 << LEDC_ResolutionRatio) - 1)

class SpiMipiLvglDisplayDriver
{
    friend void lv_lcd_send_cmd_cb(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t * param, size_t param_size);
    friend void lv_lcd_send_color_cb(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size);

    lv_display_t* _Display;
    spi_host_device_t _HostId;
    gpio_num_t _GpioCs;
    gpio_num_t _GpioDc;
    gpio_num_t _GpioRst;
    gpio_num_t _GpioBackLight;
    
    spi_device_handle_t _SpiHandle;    

    void _lv_lcd_send_cmd_cb(const uint8_t * cmd, size_t cmd_size, const uint8_t * param, size_t param_size);
    void _lv_lcd_send_color_cb(const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size);

    ledc_channel_config_t _LedcChannel;    
    void _BackLightInit();


public:
    SpiMipiLvglDisplayDriver(spi_host_device_t hostId, gpio_num_t gpioCs, gpio_num_t gpioDc, gpio_num_t gpioRst, uint32_t clockSpeedHz, gpio_num_t gpioBacklight);
    ~SpiMipiLvglDisplayDriver();

    lv_display_t* operator &();

    lv_display_t* GetDisplay() { return _Display; };

    void SetBackLight(uint8_t value);
};
