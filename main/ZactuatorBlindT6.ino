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

#define T6_ZERO_HIGH 320
#define T6_ZERO_LOW 768
#define T6_ONE_HIGH 704
#define T6_ONE_LOW 384
#define T6_START_HIGH 4800
#define T6_START_LOW 1536
#define T6_TRY_DELAY 7800
#ifndef T6_TRIES
    #define T6_TRIES 2
#endif
#define T6_CMD_OPEN 0x33
#define T6_CMD_CLOSE 0x11
#define T6_CMD_STOP 0x55
#define T6_CMD_LIGHT 0x0F

#define T6_PUB_PERIOD 10000

// #define T6_CLOSED_SWITCH_PIN 34
// #define T6_OPEN_SWITCH_PIN 35
// #define T6_LED_ON_PIN 14

uint32_t blindT6_id = T6_ID;
uint32_t blindT6_unit = T6_UNIT;
unsigned long blindT6_lastRead = 0;

#ifdef T6_CLOSED_SWITCH_PIN
bool blindT6_ClosedSwitchState = false;
#endif

#ifdef T6_OPEN_SWITCH_PIN
bool blindT6_OpenSwitchState = false;
#endif

#ifdef T6_LED_ON_PIN
bool blindT6_LightState = false;
#endif


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
    //Setup I/O if set in configuration
#ifdef T6_CLOSED_SWITCH_PIN
    pinMode(T6_CLOSED_SWITCH_PIN, INPUT_PULLUP);
#endif
#ifdef T6_OPEN_SWITCH_PIN
    pinMode(T6_OPEN_SWITCH_PIN, INPUT_PULLUP);
#endif
#ifdef T6_LED_ON_PIN
    pinMode(T6_LED_ON_PIN, INPUT);
#endif
    Log.trace(F("ZactuatorBlindT6 setup done " CR));
}

void activateLight(bool state)
{
#ifdef T6_LED_ON_PIN
    bool lightValue = digitalRead(T6_LED_ON_PIN);
    if(state != lightValue){
        sendCommandT6(T6_CMD_LIGHT);
    }
#else
    //No feed back, toggle light
    sendCommandT6(T6_CMD_LIGHT);
#endif
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
        }else if (strstr(datacallback, "light_on") != NULL) {
            activateLight(true);
        }else if (strstr(datacallback, "light_off") != NULL) {
            activateLight(false);
        }else if (strstr(datacallback, "stop") != NULL) {
            sendCommandT6(T6_CMD_STOP);
        }
        stateBlindT6Measures();
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
            }else if (strstr(command, "light_on") != NULL) {
                activateLight(true);
            }else if (strstr(command, "light_off") != NULL) {
                activateLight(false);
            }else if (strstr(command, "stop") != NULL) {
                sendCommandT6(T6_CMD_STOP);
            }
        }
        blindT6_lastRead = 0;
        stateBlindT6Measures();
    }
}
#endif

void stateBlindT6Measures()
{
#if defined(T6_CLOSED_SWITCH_PIN) || defined(T6_OPEN_SWITCH_PIN) || defined(T6_LED_ON_PIN)
    bool publish = false;
    unsigned long now = millis();
    if((now - blindT6_lastRead) >= T6_PUB_PERIOD){
#ifdef T6_CLOSED_SWITCH_PIN
        bool closedValue = digitalRead(T6_CLOSED_SWITCH_PIN);
        if(closedValue != blindT6_ClosedSwitchState){
            publish = true;
        }
        blindT6_ClosedSwitchState = closedValue;
#endif
#ifdef T6_OPEN_SWITCH_PIN
        bool openValue = digitalRead(T6_OPEN_SWITCH_PIN);
        if(openValue != blindT6_OpenSwitchState){
            publish = true;
        }
        blindT6_OpenSwitchState = openValue;
#endif
#ifdef T6_LED_ON_PIN
#ifdef T6_LED_ON_INVERTED
        bool lightValue = !digitalRead(T6_LED_ON_PIN);
#else
        bool lightValue = digitalRead(T6_LED_ON_PIN);
#endif
        if(lightValue != blindT6_LightState){
            publish = true;
        }
        blindT6_LightState = lightValue;
#endif
        publish = true;
        if(publish){
            StaticJsonDocument<JSON_MSG_BUFFER> dataBuffer;
            JsonObject valueData = dataBuffer.to<JsonObject>();
#ifdef T6_CLOSED_SWITCH_PIN
            valueData["closed"] = closedValue;
#endif
#ifdef T6_OPEN_SWITCH_PIN
            valueData["open"] = openValue;
#endif
#ifdef T6_LED_ON_PIN
            valueData["light"] = lightValue;
#endif
            pub(subjectBlindT6toMQTT, valueData);
        }
        blindT6_lastRead = now;
    }
#endif
}

#endif