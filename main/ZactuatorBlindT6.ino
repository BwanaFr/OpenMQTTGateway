#include "User_config.h"

#ifdef ZactuatorBlindT6

#include <rtl_433_ESP.h>
#include <config_BlindT6.h>
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

#define T6_ZERO_HIGH 400
#define T6_ZERO_LOW 800
#define T6_ONE_HIGH 800
#define T6_ONE_LOW 300
#define T6_START_HIGH 4200
#define T6_START_LOW 1500
#define T6_TRY_DELAY 7800
#ifndef T6_TRIES
    #define T6_TRIES 1
#endif
#define T6_CMD_OPEN 0x33
#define T6_CMD_CLOSE 0x11
#define T6_CMD_STOP 0x55
#define T6_CMD_LIGHT 0x0F

uint32_t blindT6_id = T6_ID;
uint32_t blindT6_unit = T6_UNIT;

void pulseT6(bool value, uint32_t us)
{
    digitalWrite(RF_MODULE_DIO2, value ? 1 : 0);
    delayMicroseconds(us);
}


void sendBitsT6(uint32_t bits, uint count)
{
    for(uint i=0;i<count;++i){
        uint8_t bit = (bits >> ((count-1)-i)) & 0x1;
        pulseT6(1, (bit == 1 ? T6_ONE_HIGH : T6_ZERO_HIGH));
        pulseT6(0, (bit == 1 ? T6_ONE_LOW : T6_ZERO_LOW));
    }
}

void sendCommandT6(uint8_t command)
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

    for(int j=0;j<T6_TRIES;++j){
        if(j > 0){
            //Wait 7.8ms before next
            delayMicroseconds(T6_TRY_DELAY);
        }
        //Pulse high for ~4.8ms
        pulseT6(1, T6_START_HIGH);
        //Pulse low for ~1.3ms
        pulseT6(0, T6_START_LOW);
        //Send ID
        sendBitsT6(blindT6_id, 28);
        //Send unit
        sendBitsT6(blindT6_unit, 4);
        //Send command
        sendBitsT6(command, 8);
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

void setupBlindT6()
{
    //Nothing really important to setup
    Log.trace(F("ZactuatorBlindT6 setup done " CR));
}

void blindT6toMQTT()
{
    StaticJsonDocument<JSON_MSG_BUFFER> dataBuffer;
    JsonObject valueData = dataBuffer.to<JsonObject>();
    valueData["closed"] = false;
    pub(subjectBlindT6toMQTT, valueData);
}

void MQTTtoBlindT6(char* topicOri, char* datacallback)
{
    if (cmpToMainTopic(topicOri, subjectMQTTtoBlindT6)) {
        if (strstr(datacallback, "open") != NULL) {
            sendCommandT6(T6_CMD_OPEN);
        }else if (strstr(datacallback, "close") != NULL) {
            sendCommandT6(T6_CMD_CLOSE);
        }else if (strstr(datacallback, "light") != NULL) {
            sendCommandT6(T6_CMD_LIGHT);
        }
        blindT6toMQTT();
    }
}

#if jsonReceiving
void MQTTtoBlindT6(char* topicOri, JsonObject& jsonData)
{
    if (cmpToMainTopic(topicOri, subjectMQTTtoBlindT6)) {
        if(jsonData["id"]){
            blindT6_id = jsonData["id"];
        }
        if(jsonData["unit"]){
            blindT6_unit = jsonData["unit"];
        }
        if(jsonData["command"]){
            const char* command = jsonData["command"];
            if (strstr(command, "open") != NULL) {
                sendCommandT6(T6_CMD_OPEN);
            }else if (strstr(command, "close") != NULL) {
                sendCommandT6(T6_CMD_CLOSE);
            }else if (strstr(command, "light") != NULL) {
                sendCommandT6(T6_CMD_LIGHT);
            }
        }
        blindT6toMQTT();
    }
}
#endif

#endif
