#ifndef __ACCESS_POINT_H
#define __ACCESS_POINT_

#define WLAN_ESSID "WLAN (ESSID)"
#define WLAN_PASSWD "WLAN (Password)"
#define HEIZPATRONE "Leistung Heizpatrone (W)"
#define HYSTERESE "Regelbereich Hysterese"
#define EINSPEISEBESCHRAENKUNG "Einspeisebeschraenkung (%)"
#define MINDEST_LAUFZEIT "Mindest Laufzeit (min) (%)"
#define AUSSCHALT_TEMP "Ausschalt-Temperatur (in Grad Celisius)"
#define PID_P "Ausgangsregler (P-Anteil)"
#define PID_I "Ausgangsregler (I-Anteil)"
#define PID_D "Ausgangsregler (D-Anteil)"

void ap_init();
void ap_run(); 


#endif