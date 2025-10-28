#include "NvsSettingsAccessor.h"
#include "stdint.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "NvsSettingsAccessor";

nvs_handle_t _NvsSettingsAccessorHandle = 0;
#define NULL_STR "NULL"

void NvsSettingsAccessor::Init4Read()
{
    _Init(false);
}

void NvsSettingsAccessor::Init4Write()
{
    _Init(true);
}

void NvsSettingsAccessor::_Init(bool writeMode)
{
    // Initialize NVS
    auto err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS)... Write mode %i => ", writeMode);
    err = nvs_open("storage", writeMode ? NVS_READWRITE : NVS_READONLY, &_NvsSettingsAccessorHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle(%lu)!", esp_err_to_name(err), _NvsSettingsAccessorHandle);
    } else {
        ESP_LOGI(TAG, "Done(%lu)", _NvsSettingsAccessorHandle);
    }
}

void NvsSettingsAccessor::DeInit()
{
    if (_NvsSettingsAccessorHandle == 0)
        return;
    // Close
    nvs_close(_NvsSettingsAccessorHandle);        
    _NvsSettingsAccessorHandle = 0;
}

NvsSettingsAccessor::ConnectionModes NvsSettingsAccessor::GetConnectionMode()
{
    return (ConnectionModes)_GetInt8("Settings.SsMode");
}

bool NvsSettingsAccessor::SetConnectionMode(ConnectionModes mode)
{
    return _SetInt8("Settings.SsMode", (int8_t)mode);
}

int NvsSettingsAccessor::GetBootCounter()
{
    return _GetInt32("Settings.Boots");
}

int NvsSettingsAccessor::IncBootCounter()
{
    auto boots = GetBootCounter();
    if (boots > 0)
        boots++;
    else 
        boots = 1;
    return _SetInt32("Settings.Boots", boots);
}

int NvsSettingsAccessor::GetLogsCounter()
{
    return _GetInt32("Settings.Logs");
}

int NvsSettingsAccessor::IncLogsCounter()
{
    auto boots = GetLogsCounter();
    if (boots > 0)
        boots++;
    else 
        boots = 1;
    return _SetInt32("Settings.Logs", boots);
}

const char* NvsSettingsAccessor::_GetStr(const char* keyName, char* valBuff, int sizeOfVal)
{
    if (_NvsSettingsAccessorHandle == 0)
        return NULL;

    ESP_LOGI(TAG, "=>_GetStr %s", keyName);

    size_t len = sizeOfVal-1;
    auto err = nvs_get_str(_NvsSettingsAccessorHandle, keyName,  valBuff, &len);
    valBuff[sizeOfVal-1] = 0;
    switch (err) {
    case ESP_OK:
        ESP_LOGI(TAG, "Done");
        ESP_LOGI(TAG, "%s = %s", keyName, valBuff);
        return  valBuff;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "The value %s is not initialized yet!", keyName);
        break;
    default :
        ESP_LOGI(TAG, "Error (%s) reading!", esp_err_to_name(err));
    }
    return NULL;
}

bool NvsSettingsAccessor::_SetStr(const char* keyName, const char* value)
{
    ESP_LOGI(TAG, "=>SetStr %s=%s", keyName, value);

    if (_NvsSettingsAccessorHandle == 0)
        return false;
    auto err = nvs_set_str(_NvsSettingsAccessorHandle, keyName, value);
    if (err == ESP_OK)
    {
        nvs_commit(_NvsSettingsAccessorHandle);
        return true;
    }

    ESP_LOGI(TAG, "Error (%s:%x) writing!", esp_err_to_name(err), err);
    
    return false;
}

int8_t NvsSettingsAccessor::_GetInt8(const char* keyName)
{
    if (_NvsSettingsAccessorHandle == 0)
        return 0;

    int8_t res = 0;
    auto err = nvs_get_i8(_NvsSettingsAccessorHandle, keyName, &res);
    switch (err) {
    case ESP_OK:
        ESP_LOGI(TAG, "Done");
        ESP_LOGI(TAG, "%s = %i", keyName,  res);
        return  res;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "The value '%s' is not initialized yet!", keyName);
        break;
    default :
        ESP_LOGI(TAG, "Error (%s) reading!", esp_err_to_name(err));
    }
    return res;
}

bool NvsSettingsAccessor::_SetInt8(const char* keyName, const int8_t value)
{
    ESP_LOGI(TAG, "=>SetInt8 %s=%i", keyName, value);

    if (_NvsSettingsAccessorHandle == 0)
        return false;
    auto err = nvs_set_i8(_NvsSettingsAccessorHandle, keyName, value);
    if (err == ESP_OK)
    {
        nvs_commit(_NvsSettingsAccessorHandle);
        return true;
    }

    ESP_LOGI(TAG, "Error (%s:%x) writing!", esp_err_to_name(err), err);
    
    return false;
}

int32_t NvsSettingsAccessor::_GetInt32(const char* keyName)
{
    if (_NvsSettingsAccessorHandle == 0)
        return 0;

    int32_t res = 0;
    auto err = nvs_get_i32(_NvsSettingsAccessorHandle, keyName, &res);
    switch (err) {
    case ESP_OK:
        ESP_LOGI(TAG, "Done");
        ESP_LOGI(TAG, "%s = %li", keyName,  res);
        return  res;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "The value '%s' is not initialized yet!", keyName);
        break;
    default :
        ESP_LOGI(TAG, "Error (%s) reading!", esp_err_to_name(err));
    }
    return res;
}

bool NvsSettingsAccessor::_SetInt32(const char* keyName, const int32_t value)
{
    ESP_LOGI(TAG, "=>SetInt32 %s=%li", keyName, value);

    if (_NvsSettingsAccessorHandle == 0)
        return false;
    auto err = nvs_set_i32(_NvsSettingsAccessorHandle, keyName, value);
    if (err == ESP_OK)
    {
        nvs_commit(_NvsSettingsAccessorHandle);
        return true;
    }

    ESP_LOGI(TAG, "Error (%s:%x) writing!", esp_err_to_name(err), err);
    
    return false;
}

char _NvsSettingsAccessorSsIdBuff[32];

const char* NvsSettingsAccessor::GetSsId()
{
    return _GetStr("Settings.SsId", _NvsSettingsAccessorSsIdBuff, sizeof(_NvsSettingsAccessorSsIdBuff));
}

bool NvsSettingsAccessor::SetSsId(const char* ssId)
{
    return _SetStr("Settings.SsId", ssId);
}

char _NvsSettingsAccessorPwdBuff[32];

const char* NvsSettingsAccessor::GetPwd()
{
    return _GetStr("Settings.SsPwd", _NvsSettingsAccessorPwdBuff, sizeof(_NvsSettingsAccessorPwdBuff));
}

bool NvsSettingsAccessor::SetPwd(const char* pwd)
{
    return _SetStr("Settings.SsPwd", pwd);
}

char _NvsSettingsAccessorSnBuff[32];

const char* NvsSettingsAccessor::GetSerial()
{
    return _GetStr("SerialNumber", _NvsSettingsAccessorSnBuff, sizeof(_NvsSettingsAccessorSnBuff));
}

char _NvsSettingsAccessorMqttBuff[128];

const char* NvsSettingsAccessor::GetMqtt()
{
    return _GetStr("Settings.Mqtt", _NvsSettingsAccessorMqttBuff, sizeof(_NvsSettingsAccessorMqttBuff));
}

bool NvsSettingsAccessor::SetMqtt(const char* mqtt)
{
    return _SetStr("Settings.Mqtt", mqtt);
}

