#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>

#include "animation.h"
#include "temperature.h"
#include "web_server.h"
#include "wifi_ap.h"

/*
 * Resources:
 *   GET  /               web page (lib/net/www/index.html, gzipped at build time)
 *   GET  /api/status     {"uptime_ms":..,"temp_c":..|null,"animation":".."}
 *   POST /api/animation  body is an animation name ("pong" or "sine")
 */

static const uint8_t index_html_gz[] = {
#include "index.html.gz.inc"
};

static struct http_resource_detail_static index_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_STATIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		.content_encoding = "gzip",
		.content_type = "text/html",
	},
	.static_data = index_html_gz,
	.static_data_len = sizeof(index_html_gz),
};

/* Status JSON shared by both API resources; the server handles one request at a time. */
static char json_buf[128];

static size_t format_status(void)
{
	char temp[16] = "null";
	int32_t t;
	int len;

	if (temperature_get(&t)) {
		snprintf(temp, sizeof(temp), "%s%d.%d", t < 0 ? "-" : "", abs(t) / 10,
			 abs(t) % 10);
	}

	len = snprintf(json_buf, sizeof(json_buf),
		       "{\"uptime_ms\":%lld,\"temp_c\":%s,\"animation\":\"%s\"}",
		       (long long)k_uptime_get(), temp, animation_name(animation_requested()));

	return MIN((size_t)len, sizeof(json_buf) - 1);
}

static int status_handler(struct http_client_ctx *client, enum http_transaction_status status,
			  const struct http_request_ctx *request_ctx,
			  struct http_response_ctx *response_ctx, void *user_data)
{
	if (status == HTTP_SERVER_REQUEST_DATA_FINAL) {
		wifi_ap_note_activity();
		response_ctx->body = (const uint8_t *)json_buf;
		response_ctx->body_len = format_status();
		response_ctx->final_chunk = true;
	}

	return 0;
}

static struct http_resource_detail_dynamic status_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		.content_type = "application/json",
	},
	.cb = status_handler,
};

static int animation_handler(struct http_client_ctx *client, enum http_transaction_status status,
			     const struct http_request_ctx *request_ctx,
			     struct http_response_ctx *response_ctx, void *user_data)
{
	static char body[16];
	static size_t body_len;
	int id;

	if (status == HTTP_SERVER_TRANSACTION_ABORTED ||
	    status == HTTP_SERVER_TRANSACTION_COMPLETE) {
		body_len = 0;
		return 0;
	}

	/* The body may arrive in several chunks */
	if (body_len + request_ctx->data_len > sizeof(body)) {
		body_len = 0;
		return -ENOMEM;
	}

	memcpy(body + body_len, request_ctx->data, request_ctx->data_len);
	body_len += request_ctx->data_len;

	if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
		return 0;
	}

	wifi_ap_note_activity();

	id = animation_find(body, body_len);
	body_len = 0;

	if (id < 0) {
		static const char err[] = "{\"error\":\"unknown animation\"}";

		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->body = (const uint8_t *)err;
		response_ctx->body_len = sizeof(err) - 1;
		response_ctx->final_chunk = true;
		return 0;
	}

	animation_request(id);

	response_ctx->body = (const uint8_t *)json_buf;
	response_ctx->body_len = format_status();
	response_ctx->final_chunk = true;

	return 0;
}

static struct http_resource_detail_dynamic animation_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "application/json",
	},
	.cb = animation_handler,
};

static uint16_t http_port = WEB_SERVER_PORT;

HTTP_SERVICE_DEFINE(poko_http_service, NULL, &http_port, CONFIG_HTTP_SERVER_MAX_CLIENTS, 10,
		    NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(index_resource, poko_http_service, "/", &index_resource_detail);
HTTP_RESOURCE_DEFINE(status_resource, poko_http_service, "/api/status", &status_resource_detail);
HTTP_RESOURCE_DEFINE(animation_resource, poko_http_service, "/api/animation",
		     &animation_resource_detail);

int web_server_start(void)
{
	return http_server_start();
}
