/**
 * Class to control a 433Mhz fan sold in France by Leroy Merlin with name
 * Colonne d'air à poser, EQUATION, 3trix blanc 24 W, D27 cm
 * Seems to be a EDON E360 model
 * Protocol uses a two byte command coded in pulse-distance coding, withg a pulse of ~372us
 * and a dead time of ~498us for a 0, ~1493 for a 1.
 * Command (int16) to be send (MSB first) is :
 * [0..3] : Fan speed (0 to 12)
 * [4..7] : Timer value (hours)
 * [8..9] : Rotation 0 -> 0 deg, 1-> 60 deg, 2 -> 120 deg, 3 -> 180 deg
 * [10..11] : Mode 0-> Normal, 1-> night, 2-> Boost, 3 -> Natural flow (open window)
 * [12] : Set when ON
 * [13] : Set to switch ON
*/

#ifndef _EDON_FAN_H_
#define _EDON_FAN_H_

extern void setupEdonFan();
extern void MQTTtoEdonFan(char* topicOri, char* datacallback);
extern void MQTTtoEdonFan(char* topicOri, JsonObject& RFdata);
extern void stateEdonFanMeasures();

#define subjectMQTTtoEdonFan    "/commands/MQTTtoEdonFan"
#define subjectEdonFantoMQTT    "/EdonFanToMQTT/status"


#endif //_EDON_FAN_H_
