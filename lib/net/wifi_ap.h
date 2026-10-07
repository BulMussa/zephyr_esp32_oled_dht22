#ifndef WIFI_AP_H_
#define WIFI_AP_H_

/* Access point settings */
#define WIFI_AP_SSID    "Poko"
/* WPA2 password, 8 to 63 characters */
#define WIFI_AP_PSK     "chichi1!"
#define WIFI_AP_IP      "192.168.4.1"
#define WIFI_AP_NETMASK "255.255.255.0"

/*
 * Start a WPA2 Wi-Fi access point named WIFI_AP_SSID, password WIFI_AP_PSK. The board gets the
 * fixed address WIFI_AP_IP, and a DHCP server hands out addresses to
 * devices that join. Returns 0 or a negative errno code.
 */
int wifi_ap_start(void);

#endif /* WIFI_AP_H_ */
