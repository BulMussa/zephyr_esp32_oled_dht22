#ifndef WIFI_AP_H_
#define WIFI_AP_H_

/* Access point settings */
#define WIFI_AP_SSID    "Poko"
/* WPA2 password, 8 to 63 characters */
#define WIFI_AP_PSK     "chichi1!"
#define WIFI_AP_IP      "192.168.4.1"
#define WIFI_AP_NETMASK "255.255.255.0"

/* Only one device can be connected at a time */
#define WIFI_AP_MAX_STATIONS 1

/*
 * A connected device is disconnected after this long without a request to
 * the web server, and after this long without any Wi-Fi traffic.
 */
#define WIFI_AP_IDLE_TIMEOUT_S 60

/*
 * Start a WPA2 Wi-Fi access point named WIFI_AP_SSID, password WIFI_AP_PSK. The board gets the
 * fixed address WIFI_AP_IP, and a DHCP server hands out addresses to
 * devices that join. Returns 0 or a negative errno code.
 */
int wifi_ap_start(void);

/*
 * Note that the connected device did something (e.g. an HTTP request), which
 * restarts its idle timeout. Safe to call from any thread.
 */
void wifi_ap_note_activity(void);

#endif /* WIFI_AP_H_ */
