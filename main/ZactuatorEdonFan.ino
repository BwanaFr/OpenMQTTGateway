#include "User_config.h"

#ifdef ZactuatorEdonFan

#include <rtl_433_ESP.h>
#include <config_EdonFan.h>
#include <config_RF.h>

#ifndef ZgatewayRTL_433
    #error "Only ZgatewayRTL_433 is supported (for now)!"
#endif

#ifdef RF_SX1276
    extern SX1276 radio;
#elif defined(RF_SX1278)
    extern SX1278 radio;
#else
    #error "Only SX1276 or SX1278 modules are supported!"
#endif


#define BIT_START_PULSE 372
#define BIT_0_PULSE 1493
#define BIT_1_PULSE 498

#define START_PREAMBLE_1 9201
#define START_PREAMBLE_0 2614
#define END_PULSE_0 4850

#define SEND_TRIES 20
#define EDON_PUB_PERIOD 10000

enum Rotation {DEG_ZERO = 0, DEG_60, DEG_120, DEG_180};
enum Mode {NORMAL = 0, NIGHT, BOOST, NATURAL_FLOW};

bool edonFan_On = false;
uint8_t edonFan_Speed = 5;
uint8_t edonFan_Timer = 0;
Rotation edonFan_Rotation = DEG_ZERO;
Mode edonFan_Mode = NORMAL;
static unsigned long lastEdonRead = 0;

uint16_t edonFanBuildCommand(bool startOn, bool stop)
{
    uint16_t ret = 0;
    if(stop){
        ret = 0;
        edonFan_On = false;
    }else{
        if(startOn){
            edonFan_On = true;
            ret |= (1 << 13);
        }
        if(edonFan_On){
            ret |= (1 << 12);
        }
        ret |= ((edonFan_Mode & 0x3) << 10);
        ret |= ((edonFan_Rotation & 0x3) << 8);
        ret |= ((edonFan_Timer & 0xF) << 4);
        ret |= (edonFan_Speed & 0xF);
    }
    return ret;
}

void edonFanPulse(bool value, uint32_t us)
{
    digitalWrite(RF_MODULE_DIO2, value ? 1 : 0);
    delayMicroseconds(us);
}

void edonFanSendBit(bool value)
{    //Send pulse
    edonFanPulse(1, BIT_START_PULSE);
    edonFanPulse(0, value ? BIT_1_PULSE : BIT_0_PULSE);
}

void edonFanSendCommand(uint16_t command)
{
//Disable receiver
    rtl_433.disableReceiver();
    //Gets datashaping from module (using SPI)
    int16_t dataShaping = radio.getMod()->SPIgetRegValue(RADIOLIB_SX127X_REG_PA_RAMP, 6, 5) >> 5;
    radio.setDataShapingOOK(0);

#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, HIGH);
#endif
    if(radio.transmitDirect() != RADIOLIB_ERR_NONE){
        Log.error(F("ZactuatorBlindT6 Radiolib error!" CR));
        return;
    }

    pinMode(RF_MODULE_DIO2, OUTPUT);    //Data pin

    for(int j=0;j<SEND_TRIES;++j){
        //Pulse high for ~8ms
        edonFanPulse(1, START_PREAMBLE_1);
        //Pulse low for ~2550us
        edonFanPulse(0, START_PREAMBLE_0);
        //Send start bit
        edonFanSendBit(0);
        //Send the 15 bit command
        for(int i=14;i >= 0;--i){
            uint16_t cmd = (command >> i) & 0x1;
            edonFanSendBit(cmd);
        }
        //Send stop bit
        edonFanPulse(1, BIT_START_PULSE);
        edonFanPulse(0, END_PULSE_0);
    }
    //Put receiver back to normal mode
    pinMode(RF_MODULE_DIO2, INPUT);
    radio.setDataShapingOOK(dataShaping);
    radio.receiveDirect();
    rtl_433.enableReceiver();
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, LOW);
#endif
}

void setupEdonFan()
{
    //Nothing to setup here
    Log.trace(F("ZactuatorEdonFan setup done " CR));
}

void MQTTtoEdonFan(char* topicOri, char* datacallback)
{
    if (cmpToMainTopic(topicOri, subjectMQTTtoEdonFan)) {
        char* command = NULL;
        int value = 0;
        int nbParams = sscanf(datacallback, "%s %d", command, &value);
        if(nbParams > 0){
            bool sendCommand = false;
            if (strstr(datacallback, "on") != NULL) {
                edonFanSendCommand(edonFanBuildCommand(true, false));
            }else if (strstr(datacallback, "off") != NULL) {
                edonFanSendCommand(edonFanBuildCommand(false, true));
            }else if ((strstr(datacallback, "speed") != NULL) && (nbParams > 1)) {
                edonFan_Speed = value;
                sendCommand = true;
            }else if ((strstr(datacallback, "mode") != NULL) && (nbParams > 1)) {
                edonFan_Mode = (Mode)value;
                sendCommand = true;
            }else if ((strstr(datacallback, "rotation") != NULL) && (nbParams > 1)) {
                edonFan_Rotation = (Rotation)value;
                sendCommand = true;
            }else if ((strstr(datacallback, "timer") != NULL) && (nbParams > 1)) {
                edonFan_Timer = value;
                sendCommand = true;
            }
            edonFanSendCommand(edonFanBuildCommand(false, false));
        }
    }
}

void MQTTtoEdonFan(char* topicOri, JsonObject& jsonData)
{
    if (cmpToMainTopic(topicOri, subjectMQTTtoEdonFan)) {
        bool sendCommand = false;
        if(jsonData["speed"]){
            int speed = jsonData["speed"];
            edonFan_Speed = speed;
            sendCommand = true;
        }
        if(jsonData["timer"]){
            int timer = jsonData["timer"];
            edonFan_Timer = timer;
            sendCommand = true;
        }
        if(jsonData["mode"]){
            if(jsonData["mode"].is<const char*>()){
                const char* mode = jsonData["mode"];
                if(strstr(mode, "NORMAL")){
                    edonFan_Mode = NORMAL;
                }else if(strstr(mode, "NIGHT")){
                    edonFan_Mode = NIGHT;
                }else if(strstr(mode, "BOOST")){
                    edonFan_Mode = BOOST;
                }else if(strstr(mode, "NATURAL")){
                    edonFan_Mode = NATURAL_FLOW;
                }
                sendCommand = true;
            }
        }

        if(jsonData["rotation"]){
            if(jsonData["rotation"].is<const char*>()){
                const char* rotation = jsonData["rotation"];
                if(strstr(rotation, "ZERO")){
                    edonFan_Rotation = DEG_ZERO;
                }else if(strstr(rotation, "60")){
                    edonFan_Rotation = DEG_60;
                }else if(strstr(rotation, "120")){
                    edonFan_Rotation = DEG_120;
                }else if(strstr(rotation, "180")){
                    edonFan_Rotation = DEG_180;
                }
                sendCommand = true;
            }
        }

        if(jsonData["on"]){
            sendCommand = false;
            edonFanSendCommand(edonFanBuildCommand(true, false));
        }
        if(jsonData["off"]){
            sendCommand = false;
            edonFanSendCommand(edonFanBuildCommand(false, true));
        }
        if(sendCommand){
            edonFanSendCommand(edonFanBuildCommand(false, false));
        }
        lastEdonRead = 0;
        stateEdonFanMeasures();
    }
}

void stateEdonFanMeasures()
{
    
    unsigned long now = millis();
    if((now - lastEdonRead) >= EDON_PUB_PERIOD){
        lastEdonRead = now;
        StaticJsonDocument<JSON_MSG_BUFFER> dataBuffer;
        JsonObject valueData = dataBuffer.to<JsonObject>();
        valueData["state"] = edonFan_On ? "ON" : "OFF";
        switch(edonFan_Mode){
            case NORMAL:
                valueData["mode"] = "NORMAL";
                break;
            case NIGHT:
                valueData["mode"] = "NIGHT";
                break;
            case BOOST:
                valueData["mode"] = "BOOST";
                break;
            case NATURAL_FLOW:
                valueData["mode"] = "NATURAL";
                break;
        }
        switch(edonFan_Rotation){
            case DEG_ZERO:
                valueData["rotation"] = "ZERO";
                break;
            case DEG_60:
                valueData["rotation"] = "60";
                break;
            case DEG_120:
                valueData["rotation"] = "120";
                break;
            case DEG_180:
                valueData["rotation"] = "180";
                break;
        }
        valueData["speed"] = edonFan_Speed;
        valueData["timer"] = edonFan_Timer;
        pub(subjectEdonFantoMQTT, valueData);
    }
}

#endif
