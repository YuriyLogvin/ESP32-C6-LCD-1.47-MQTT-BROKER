#include <stdio.h>
#include <lvgl.h>
#include <font/lv_font.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include <nvs_flash.h>

#include <Arduino.h>
#include "PicoMQTT.h"

#include "SpiMipiLvglDisplayDriver.h"
#include "NvsSettingsAccessor.h"

#include "esp_console.h"
#include "esp_system.h"

static const char *TAG = "MAIN";

#define DEFAULT_WIFI_SSID       "TestWiFi"
#define DEFAULT_WIFI_PWD        "12345678"

#define ELMB_LCD_HOST  SPI2_HOST
#define ELMB_LCD_PIXEL_CLOCK_HZ     (12 * 1000 * 1000)
#define ELMB_PIN_NUM_SCLK           GPIO_NUM_7
#define ELMB_PIN_NUM_MOSI           GPIO_NUM_6
#define ELMB_PIN_NUM_MISO           GPIO_NUM_5
#define ELMB_PIN_NUM_LCD_CS         GPIO_NUM_14
#define ELMB_PIN_NUM_LCD_DC         GPIO_NUM_15
#define ELMB_PIN_NUM_LCD_RST        GPIO_NUM_21
#define ELMB_PIN_NUM_BK_LIGHT       GPIO_NUM_22


void _CreateConsoleCommands();
void _SpiInit();    
void _StartMqttBrocker();
void _StartWiFi();
void _CreateUI();
void _LoadSettings();

PicoMQTT::Server _Mqtt;

const char* _SsIdName = DEFAULT_WIFI_SSID;
const char* _SsIdPwd = DEFAULT_WIFI_PWD;
NvsSettingsAccessor::ConnectionModes _SsIdMode = NvsSettingsAccessor::ConnectionModes::AP;

extern "C" void app_main()
{
    ESP_LOGI(TAG, "->main");

    _LoadSettings(); 

    //Init SPI common driver for LVGL and SD card if used
    _SpiInit();

    //Init LVGL for common SPI driver
    SpiMipiLvglDisplayDriver spiDisplayDriver(ELMB_LCD_HOST, ELMB_PIN_NUM_LCD_CS, ELMB_PIN_NUM_LCD_DC, ELMB_PIN_NUM_LCD_RST, ELMB_LCD_PIXEL_CLOCK_HZ, ELMB_PIN_NUM_BK_LIGHT);

    _StartWiFi();    

    //Initialize MQTT broker
    _StartMqttBrocker();    

    //Create a label on the screen and link them to mqtt topics
    _CreateUI();

    // Create console commands for configuring WiFi  
    _CreateConsoleCommands();

    int i = 0;

    ESP_LOGI(TAG, "->while");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1));

        lv_timer_handler();
        _Mqtt.loop();

        //if (random(1000) == 0)
        //    _Mqtt.publish("picomqtt/welcome", "Hello from PicoMQTT!");

        /*if (++i % 50 == 0) 
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d%%", i/50);
            lv_label_set_text(label, buf);
            lv_obj_center(label);
           // ESP_LOGI(TAG, "Running lv_timer_handler... %d", i/50);
        }*/
    }


}

void _LoadSettings()
{
    _SsIdMode = NvsSettingsAccessor::ConnectionModes::AP;
    NvsSettingsAccessor::Init4Read();        
    _SsIdName = NvsSettingsAccessor::GetSsId();
    if (_SsIdName == NULL)
    {
        _SsIdName = DEFAULT_WIFI_SSID;
    }
    else
    {
        _SsIdPwd = NvsSettingsAccessor::GetPwd();
        _SsIdMode = NvsSettingsAccessor::GetConnectionMode();
    }
    NvsSettingsAccessor::DeInit();
}

int OnSetSSID(int argc, char **argv)
{
    if (argc < 3) {

        NvsSettingsAccessor::Init4Read();        

        auto ssId = NvsSettingsAccessor::GetSsId();
        auto mode = NvsSettingsAccessor::GetConnectionMode();

        NvsSettingsAccessor::DeInit();

        printf("SSID=%s; MODE=%d\n", ssId != NULL ? ssId : DEFAULT_WIFI_SSID, mode == NvsSettingsAccessor::ConnectionModes::Client ? 1 : 0);

        printf("Usage: SSID <SSID> <SSID PWD> <MODE>\n");
        printf("\t<MODE>: 0-AP(default), 1-Client\n");
        return 1;
    }

    const char *ssid = argv[1];
    const char *ssidPwd = argv[2];
    NvsSettingsAccessor::ConnectionModes mode = NvsSettingsAccessor::ConnectionModes::AP;
    if (argc > 3)
    {
        int modeInt = atoi(argv[3]);
        if (modeInt == 1)
            mode = NvsSettingsAccessor::ConnectionModes::Client;
    }   
    printf("Setting SSID to '%s' mode %d\n", ssid, mode == NvsSettingsAccessor::ConnectionModes::Client ? 1 : 0);

    NvsSettingsAccessor::Init4Write();        

    NvsSettingsAccessor::SetSsId(ssid);
    NvsSettingsAccessor::SetPwd(ssidPwd);
    NvsSettingsAccessor::SetConnectionMode(mode);

    NvsSettingsAccessor::DeInit();

    printf("Restart device for apply changes\n");

    return 0;
}

void _CreateConsoleCommands()
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = "config>";
    repl_config.max_cmdline_length = 256;

    static esp_console_cmd_t cmd = {
        .command = "SSID",
        .help = "Set WiFi SSID and Password",
        .hint = NULL,
        .func = OnSetSSID,
        .argtable = NULL
    };

    /* Register commands */
    esp_console_register_help_command();
    esp_console_cmd_register(&cmd);
    //register_system_common();

    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

}

void _SpiInit()
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = ELMB_PIN_NUM_MOSI,
        .miso_io_num = ELMB_PIN_NUM_MISO,
        .sclk_io_num = ELMB_PIN_NUM_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 48000,
    };

    auto ret = spi_bus_initialize(ELMB_LCD_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus.");
    }
}

void _StartWiFi()
{
    ESP_LOGI(TAG, "Starting Wifi with SSID '%s'...", _SsIdName);
    switch (_SsIdMode)
    {
        case NvsSettingsAccessor::ConnectionModes::Client:
            ESP_LOGI(TAG, "\tSTA Mode");
            WiFi.mode(WIFI_STA);
            break;
        case NvsSettingsAccessor::ConnectionModes::AP:
        default:
            ESP_LOGI(TAG, "\AP Mode");
            WiFi.mode(WIFI_AP);
            break;
    }

    auto res = WiFi.begin(_SsIdName, _SsIdPwd);
    ESP_LOGI(TAG, "Started %d...", res);

}

void _StartMqttBrocker()
{
    ESP_LOGI(TAG, "Starting MQTT broker...");

    _Mqtt.keep_alive_tolerance_millis = 20000;
    _Mqtt.socket_timeout_millis = 15000;

    _Mqtt.begin();


    ESP_LOGI(TAG, "MQTT broker started.");
}

void _CreateUI()
{
    static lv_font_t _LargeFont = lv_font_montserrat_40;
    static lv_font_t _MidFont = lv_font_montserrat_28;
    static lv_font_t _SmallFont = lv_font_montserrat_14;

    static lv_obj_t * labelSoc = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(labelSoc, &_LargeFont, 0);    
    lv_label_set_text(labelSoc, "---");
    lv_obj_align(labelSoc, LV_ALIGN_CENTER, 0, 40);
    _Mqtt.subscribe("emkit/+/+/socofpack", [](const char * topic, const char * payload) {
            std::string payloadStr(payload);
            payloadStr+="%";
            lv_label_set_text(labelSoc, payloadStr.c_str());
        });    

    static lv_obj_t * labelBatVoltage = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(labelBatVoltage, &_LargeFont, 0);    
    lv_label_set_text(labelBatVoltage, "---");
    lv_obj_align(labelBatVoltage, LV_ALIGN_CENTER, 0, -40);
    _Mqtt.subscribe("emkit/+/+/voltageofpack", [](const char * topic, const char * payload) {
            std::string payloadStr(payload);
            payloadStr+="V";
            lv_label_set_text(labelBatVoltage, payloadStr.c_str());
        });    

    static lv_obj_t * labelBatCurrent = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(labelBatCurrent, &_LargeFont, 0);    
    lv_label_set_text(labelBatCurrent, "---");
    lv_obj_align(labelBatCurrent, LV_ALIGN_CENTER, 0, 0);
    _Mqtt.subscribe("emkit/+/+/currentofpack", [](const char * topic, const char * payload) {
            std::string payloadStr(payload);
            payloadStr+="A";
            lv_label_set_text(labelBatCurrent, payloadStr.c_str());
        });    

    static lv_obj_t * labelMaxCellVoltage = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(labelMaxCellVoltage, &_MidFont, 0);    
    lv_label_set_text(labelMaxCellVoltage, "     ");
    lv_obj_align(labelMaxCellVoltage, LV_ALIGN_CENTER, 0, -140);
    _Mqtt.subscribe("emkit/+/+/cellvmax", [](const char * topic, const char * payload) {
            std::string payloadStr(payload);
            payloadStr+="V";
            lv_label_set_text(labelMaxCellVoltage, payloadStr.c_str());
        });    

    static lv_obj_t * labelMaxCellTemp = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(labelMaxCellTemp, &_MidFont, 0);    
    lv_label_set_text(labelMaxCellTemp, "     ");
    lv_obj_align(labelMaxCellTemp, LV_ALIGN_CENTER, 0, -110);
    _Mqtt.subscribe("emkit/+/+/celltmax", [](const char * topic, const char * payload) {
            std::string payloadStr(payload);
            payloadStr+="°";
            lv_label_set_text(labelMaxCellTemp, payloadStr.c_str());
        });    


    static lv_obj_t * labelMinCellVoltage = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(labelMinCellVoltage, &_MidFont, 0);    
    lv_label_set_text(labelMinCellVoltage, "     ");
    lv_obj_align(labelMinCellVoltage, LV_ALIGN_CENTER, 0, 140);
    _Mqtt.subscribe("emkit/+/+/cellvmin", [](const char * topic, const char * payload) {
            std::string payloadStr(payload);
            payloadStr+="V";
            lv_label_set_text(labelMinCellVoltage, payloadStr.c_str());
        });    

    static lv_obj_t * labelMinCellTemp = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(labelMinCellTemp, &_MidFont, 0);    
    lv_label_set_text(labelMinCellTemp, "     ");
    lv_obj_align(labelMinCellTemp, LV_ALIGN_CENTER, 0, 110);
    _Mqtt.subscribe("emkit/+/+/celltmin", [](const char * topic, const char * payload) {
            std::string payloadStr(payload);
            payloadStr+="°";
            lv_label_set_text(labelMinCellTemp, payloadStr.c_str());
        });            

    static lv_obj_t * labelChargeState = lv_label_create(lv_screen_active());
    static int _LastrChargeState = -1;
    lv_obj_set_style_text_font(labelChargeState, &_MidFont, 0);    
    lv_label_set_text(labelChargeState, "     ");
    lv_obj_align(labelChargeState, LV_ALIGN_CENTER, 0, -80);
    _Mqtt.subscribe("emkit/+/+/bmschstate", [](const char * topic, const char * payload) {
            if (_LastrChargeState != payload[0])
            {
                _LastrChargeState = payload[0];
                lv_obj_set_style_text_color(labelChargeState, lv_color_make(0, 255, 0), LV_PART_MAIN | LV_STATE_DEFAULT);
                switch (payload[0])
                {
                case '0':
                    lv_label_set_text(labelChargeState, "Disable");
                    break;
                case '1':
                    lv_label_set_text(labelChargeState, "Waiting");
                    break;
                case '2':
                    lv_obj_set_style_text_color(labelChargeState, lv_color_make(0, 0, 255), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(labelChargeState, "Charging");
                    break;
                case '3':
                    lv_label_set_text(labelChargeState, "Ballancing");
                    break;
                case '4':
                    lv_label_set_text(labelChargeState, "Charged");
                    break;
                case '5':
                    lv_label_set_text(labelChargeState, "OverCool");
                    break;
                case '6':
                    lv_label_set_text(labelChargeState, "OverHeat");
                    break;
                default:
                    lv_label_set_text(labelChargeState, "Unknown");
                    break;
                }
            }
        });            

    static lv_obj_t * labelDischargeState = lv_label_create(lv_screen_active());
    static int _LastrDischargeState = -1;
    lv_obj_set_style_text_font(labelDischargeState, &_MidFont, 0);    
    lv_label_set_text(labelDischargeState, "     ");
    lv_obj_align(labelDischargeState, LV_ALIGN_CENTER, 0, 77);
    _Mqtt.subscribe("emkit/+/+/bmsdschstate", [](const char * topic, const char * payload) {
            if (_LastrDischargeState != payload[0])
            {
                lv_obj_set_style_text_color(labelDischargeState, lv_color_make(0, 255, 0), LV_PART_MAIN | LV_STATE_DEFAULT);
                _LastrDischargeState = payload[0];
                switch (payload[0])
                {
                case '0':
                    lv_label_set_text(labelDischargeState, "Disable");
                    break;
                case '1':
                    lv_label_set_text(labelDischargeState, "Waiting");
                    break;
                case '2':
                    lv_obj_set_style_text_color(labelDischargeState, lv_color_make(0, 0, 255), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(labelDischargeState, "Discharging");
                    break;
                case '3':
                    lv_label_set_text(labelDischargeState, "Stopped");
                    break;
                case '4':
                    lv_label_set_text(labelDischargeState, "Discharged");
                    break;
                case '5':
                    lv_label_set_text(labelDischargeState, "OverCool");
                    break;
                case '6':
                    lv_label_set_text(labelDischargeState, "OverHeat");
                    break;
                default:
                    lv_label_set_text(labelDischargeState, "Unknown");
                    break;
                }
            }
        });            

}
