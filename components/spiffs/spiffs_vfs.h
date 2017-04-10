/*
 * spiffs_vfs.h
 *
 * Based on https://github.com/nkolban/esp32-snippets/tree/master/vfs/spiffs
 */

#pragma once

#include "spiffs.h"

void spiffs_register_vfs(char *mount_point, const esp_partition_t *partition);
void test_spiffs(void);
