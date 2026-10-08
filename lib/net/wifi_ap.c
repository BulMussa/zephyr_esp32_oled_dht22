#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include <esp_wifi.h>

#include "wifi_ap.h"

/* First address handed out by the DHCP server is WIFI_AP_IP + this offset */
#define DHCP_POOL_OFFSET 10

BUILD_ASSERT(sizeof(WIFI_AP_PSK) - 1 >= 8 && sizeof(WIFI_AP_PSK) - 1 <= 63,
	     "WPA2 password must be 8 to 63 characters");

#define WIFI_AP_EVENTS                                                                     \
	(NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_STA_CONNECTED |               \
	 NET_EVENT_WIFI_AP_STA_DISCONNECTED)

/* How often the idle timeout is checked while a device is connected */
#define IDLE_CHECK_INTERVAL K_SECONDS(5)

static struct net_mgmt_event_callback wifi_cb;

/* Number of connected devices and uptime (ms, 32 bit) of the last activity */
static atomic_t station_count;
static atomic_t last_activity;

static void idle_check(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(idle_work, idle_check);

void wifi_ap_note_activity(void)
{
	atomic_set(&last_activity, (atomic_val_t)k_uptime_get_32());
}

/* Runs on the system work queue while a device is connected. */
static void idle_check(struct k_work *work)
{
	uint32_t idle_ms;
	esp_err_t err;

	if (atomic_get(&station_count) == 0) {
		return;
	}

	idle_ms = k_uptime_get_32() - (uint32_t)atomic_get(&last_activity);
	if (idle_ms >= WIFI_AP_IDLE_TIMEOUT_S * MSEC_PER_SEC) {
		printk("No activity for %u s, disconnecting device\n", idle_ms / MSEC_PER_SEC);

		/* AID 0 disconnects all stations; there is at most one */
		err = esp_wifi_deauth_sta(0);
		if (err == ESP_OK) {
			/* The disconnect event stops the checks */
			return;
		}

		printk("Disconnecting device failed: %d\n", err);
	}

	k_work_reschedule(&idle_work, IDLE_CHECK_INTERVAL);
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t event,
			       struct net_if *iface)
{
	const struct wifi_ap_sta_info *sta = cb->info;

	switch (event) {
	case NET_EVENT_WIFI_AP_ENABLE_RESULT:
		printk("Wi-Fi AP \"%s\" is up, open http://%s\n", WIFI_AP_SSID, WIFI_AP_IP);
		break;
	case NET_EVENT_WIFI_AP_STA_CONNECTED:
		printk("Device joined: %02X:%02X:%02X:%02X:%02X:%02X\n", sta->mac[0],
		       sta->mac[1], sta->mac[2], sta->mac[3], sta->mac[4], sta->mac[5]);

		/* The idle timeout starts when the device joins */
		wifi_ap_note_activity();
		atomic_inc(&station_count);
		k_work_reschedule(&idle_work, IDLE_CHECK_INTERVAL);
		break;
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED:
		printk("Device left: %02X:%02X:%02X:%02X:%02X:%02X\n", sta->mac[0],
		       sta->mac[1], sta->mac[2], sta->mac[3], sta->mac[4], sta->mac[5]);

		if (atomic_get(&station_count) > 0 && atomic_dec(&station_count) == 1) {
			k_work_cancel_delayable(&idle_work);
		}
		break;
	default:
		break;
	}
}

/* Give the AP interface its fixed address and start handing out addresses. */
static int setup_ip(struct net_if *iface)
{
	struct net_in_addr addr, netmask;
	int ret;

	if (net_addr_pton(NET_AF_INET, WIFI_AP_IP, &addr) < 0 ||
	    net_addr_pton(NET_AF_INET, WIFI_AP_NETMASK, &netmask) < 0) {
		return -EINVAL;
	}

	if (net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0) == NULL) {
		return -ENOMEM;
	}

	if (!net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask)) {
		return -EINVAL;
	}

	net_if_ipv4_set_gw(iface, &addr);

	addr.s4_addr[3] += DHCP_POOL_OFFSET;
	ret = net_dhcpv4_server_start(iface, &addr);
	if (ret < 0) {
		printk("DHCP server failed to start: %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * Apply AP settings that the Zephyr Wi-Fi API does not offer for the ESP32:
 * the driver always allows 5 stations, and Wi-Fi inactivity defaults to 300 s.
 * Must be called after the AP is started.
 */
static int apply_esp_ap_settings(void)
{
	wifi_config_t config;
	esp_err_t err;

	err = esp_wifi_get_config(WIFI_IF_AP, &config);
	if (err != ESP_OK) {
		return -EIO;
	}

	config.ap.max_connection = WIFI_AP_MAX_STATIONS;

	err = esp_wifi_set_config(WIFI_IF_AP, &config);
	if (err != ESP_OK) {
		return -EIO;
	}

	err = esp_wifi_set_inactive_time(WIFI_IF_AP, WIFI_AP_IDLE_TIMEOUT_S);
	if (err != ESP_OK) {
		return -EIO;
	}

	return 0;
}

int wifi_ap_start(void)
{
	struct net_if *iface = net_if_get_wifi_sap();
	struct wifi_connect_req_params params = {
		.ssid = (const uint8_t *)WIFI_AP_SSID,
		.ssid_length = strlen(WIFI_AP_SSID),
		.psk = (const uint8_t *)WIFI_AP_PSK,
		.psk_length = strlen(WIFI_AP_PSK),
		.security = WIFI_SECURITY_TYPE_PSK,
		.channel = WIFI_CHANNEL_ANY,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
	};
	int ret;

	if (iface == NULL) {
		return -ENODEV;
	}

	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler, WIFI_AP_EVENTS);
	net_mgmt_add_event_callback(&wifi_cb);

	ret = setup_ip(iface);
	if (ret < 0) {
		return ret;
	}

	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface, &params, sizeof(params));
	if (ret < 0) {
		printk("Wi-Fi AP enable failed: %d\n", ret);
		return ret;
	}

	ret = apply_esp_ap_settings();
	if (ret < 0) {
		printk("Wi-Fi AP settings failed: %d\n", ret);
		return ret;
	}

	return 0;
}
