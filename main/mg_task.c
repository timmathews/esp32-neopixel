#include <esp_attr.h>
#include <esp_log.h>

#include "mongoose.h"
#include "mg_task.h"

DRAM_ATTR const char mg_tag[] = "mg_task";

static void handle_update(struct mg_connection *nc, int ev, void *ev_data) {
  if(ev == MG_EV_HTTP_REQUEST) {
    char color[8], animation[2];
    struct http_message *hm = (struct http_message *)ev_data;
    mg_get_http_var(&hm->body, "color", color, sizeof(color));
    mg_get_http_var(&hm->body, "animation", animation, sizeof(animation));
    ESP_LOGI(mg_tag, "color: %s, animation: %s", color, animation);
    mg_send_head(nc, 204, 0, 0);
  }
}

void ev_handler(struct mg_connection *nc, int ev, void *p) {
  struct mg_serve_http_opts opts = {
    .document_root = "/data",
    .index_files = "index.html",
    .enable_directory_listing = "no"
  };

  switch (ev) {
    case MG_EV_ACCEPT: {
      char addr[32];
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      ESP_LOGI(mg_tag, "Connection %p from %s", nc, addr);
      break;
    }
    case MG_EV_HTTP_REQUEST: {
      char addr[32];
      struct http_message *hm = (struct http_message *) p;
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      ESP_LOGI(mg_tag, "HTTP request from %s: %.*s %.*s", addr,
        hm->method.len, hm->method.p, hm->uri.len, hm->uri.p);

      if(strncmp(hm->method.p, "POST", 4) == 0) {
        char * body = malloc(hm->body.len + 1);
        strncpy(body, hm->body.p, hm->body.len);
        ESP_LOGI(mg_tag, "HTTP post: %s", body);
        mg_send_head(nc, 204, 0, 0);

        free(body);
      } else {
        mg_serve_http(nc, hm, opts);
      }

      break;
    }
    case MG_EV_CLOSE: {
      ESP_LOGI(mg_tag, "Connection %p closed", nc);
      break;
    }
  }
}

int hash(void *key) {
  return *(int *)key;
}

bool equal(void *key_a, void *key_b) {
  return *(int *)key_a == *(int *)key_b;
}

void mg_task(void *arg) {
  struct mg_mgr mgr;
  struct mg_connection *nc;

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
