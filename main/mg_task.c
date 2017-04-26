#include <esp_attr.h>
#include <esp_log.h>

#include "mongoose.h"
#include "mg_task.h"

DRAM_ATTR const char mg_tag[] = "mg_task";

void (*set_color)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);

static void broadcast_status(struct mg_connection *nc, const struct mg_str msg) {
	struct mg_connection *c;

	for(c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
		mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, msg.p, msg.len);
	}
}

static void handle_update(struct mg_connection *nc, int ev, void *ev_data) {
	if(ev == MG_EV_HTTP_REQUEST) {
		char color[8], animation[2];
		struct http_message *hm = (struct http_message *)ev_data;
		mg_get_http_var(&hm->body, "color", color, sizeof(color));
		mg_get_http_var(&hm->body, "animation", animation, sizeof(animation));
		ESP_LOGI(mg_tag, "color: %s, animation: %s", color, animation);

		long c = strtol(color + 1, (char **)NULL, 16);
		int a = atoi(animation);

		uint8_t r = c >> 16;
		uint8_t g = (c >> 8) & 0x00ff;
		uint8_t b = c & 0x0000ff;
		uint8_t w = 0;
		if(r == g && r == b) {
			w = r;
			r = g = b = 0;
		}

		set_color(r, g, b, w, a);

		char str[12];
		snprintf(str, 11, "%02x %02x %02x %02x", r, g, b, w);

		mg_send_head(nc, 204, 0, 0);

		broadcast_status(nc, mg_mk_str(str));
	}
}

void ev_handler(struct mg_connection *nc, int ev, void *p) {
	struct mg_serve_http_opts opts = {
		.document_root = "/data",
		.index_files = "index.html",
		.enable_directory_listing = "no"
	};
	struct http_message *hm = (struct http_message *) p;
	char addr[32];

	switch (ev) {
		case MG_EV_ACCEPT:
		{
			mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
					MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
			ESP_LOGI(mg_tag, "Connection %p from %s", nc, addr);
			break;
		}
		case MG_EV_HTTP_REQUEST:
		{
			mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
					MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
			ESP_LOGI(mg_tag, "HTTP request from %s: %.*s %.*s", addr,
					hm->method.len, hm->method.p, hm->uri.len, hm->uri.p);
			mg_serve_http(nc, hm, opts);
			break;
		}
		case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
		{
			broadcast_status(nc, mg_mk_str("++ joined"));
			ESP_LOGI(mg_tag, "Handshake done");
			break;
		}
		case MG_EV_WEBSOCKET_FRAME:
		{
			struct websocket_message *wm = (struct websocket_message *) p;
			struct mg_str d = {(char *) wm->data, wm->size};
			broadcast_status(nc, d);
			ESP_LOGI(mg_tag, "Frame");
			break;
		}
		case MG_EV_CLOSE:
		{
			ESP_LOGI(mg_tag, "Connection %p closed", nc);
			break;
		}
	}
}

void mg_os_task(void *arg) {
	struct mg_mgr mgr;
	struct mg_connection *nc;

	set_color = arg;

	mg_mgr_init(&mgr, NULL);

	nc = mg_bind(&mgr, MG_LISTEN_ADDR, ev_handler);
	mg_register_http_endpoint(nc, "/update", handle_update);

	if (nc == NULL) {
		ESP_LOGE(mg_tag, "Error setting up listener!");
		vTaskDelete(NULL);
		return;
	}

	mg_set_protocol_http_websocket(nc);

	while (true) {
		mg_mgr_poll(&mgr, MG_POLL);
	}
}
