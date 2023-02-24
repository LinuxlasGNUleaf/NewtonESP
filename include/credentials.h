#ifndef CREDENTIALS_H
#define CREDENTIALS_H
#define WIFI_SW 1

#if WIFI_SW == 1
inline const char* ssid = "TP-LINK_AP_F124";
inline const char* pass = "94116656";
inline const char *server = "192.168.178.73";
#elif WIFI_SW == 2
inline const char* ssid = "mRNA-Impfchip_BP7543-69420PB_5G";
inline const char* pass = "31c29a85b";
inline const char *server = "192.168.74.1";
#elif WIFI_SW == 3
inline const char* ssid = "mRNA-Impfchip_BP7543-69420PB_5G";
inline const char* pass = "31c29a85b";
inline const char *server = "192.168.74.1";
#endif

#endif