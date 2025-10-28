#pragma once

#include <stdint.h>
//#include "ConnectionModes.h"

class NvsSettingsAccessor
{
private:
    static void _Init(bool writeMode = false);
    static const char* _GetStr(const char* keyName, char* val, int sizeOfVal);
    static bool _SetStr(const char* keyName, const char* value);

    static int8_t _GetInt8(const char* keyName);
    static bool _SetInt8(const char* keyName, const int8_t value);

    static int32_t _GetInt32(const char* keyName);
    static bool _SetInt32(const char* keyName, const int32_t value);

public:

    enum class ConnectionModes
    {
        FromDeviceOrAP = 0,
        AP = 1,
        Client = 2
    };

    static void Init4Read();
    static void Init4Write();
    static void DeInit();

    static ConnectionModes GetConnectionMode();
    static bool SetConnectionMode(ConnectionModes);

    static const char* GetSsId();
    static bool SetSsId(const char*);

    static const char* GetPwd();
    static bool SetPwd(const char*);

    static const char* GetSerial();

    static const char* GetMqtt();
    static bool SetMqtt(const char*);

    static int GetBootCounter();
    static int IncBootCounter();

    static int GetLogsCounter();
    static int IncLogsCounter();
};

