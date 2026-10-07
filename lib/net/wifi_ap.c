#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/printk.h>

#include "wifi_ap.h"

/* First address handed out by the DHCP server is WIFI_AP_IP + this offset */
#define DHCP_POOL_OFFSET 10

BUILD_ASSERT(sizeof(WIFI_AP_PSK) - 1 >= 8 && sizeof(WIFI_AP_PSK) - 1 <= 63,
	     "WPA2 password must be 8 to 63 characters");

#define WIFI_AP_EVENTS                                                                     \
	(NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_STA_CONNECTED |               \
	 NET_EVENT_WIFI_AP_STA_DISCONNECTED)

static struct net_mgmt_event_callback wifi_cb;

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
		break;
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED:
		printk("Device left: %02X:%02X:%02X:%02X:%02X:%02X\n", sta->mac[0],
		       sta->mac[1], sta->mac[2], sta->mac[3], sta->mac[4], sta->mac[5]);
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

	return 0;
}
