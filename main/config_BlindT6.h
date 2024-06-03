/*
    Blind T6
    Controls my store with blind T6 protocol
*/

extern void setupBlindT6();
extern void blindT6toMQTT();
extern void MQTTtoBlindT6(char* topicOri, char* datacallback);
extern void MQTTtoBlindT6(char* topicOri, JsonObject& RFdata);

#define subjectMQTTtoBlindT6    "/commands/MQTTtoBlindT6"
#define subjectBlindT6toMQTT    "/MQTTtoBlindT6/status"
#define subjectMQTTtoBlindT6set "/commands/MQTTtoBlindT6/config"

#ifndef T6_ID
    #define T6_ID 0x9A276B0
#endif

#ifndef T6_UNIT
    #define T6_UNIT 0x5
#endif
