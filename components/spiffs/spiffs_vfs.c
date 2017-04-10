/*
 * spiffs_vfs.c
 *
 * Based on https://github.com/nkolban/esp32-snippets/tree/master/vfs/spiffs
 */

#include <esp_vfs.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include "spiffs.h"

#define LOG_PAGE_SIZE       256

static uint8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static uint8_t spiffs_fds[32*4];
static uint8_t spiffs_cache_buf[(LOG_PAGE_SIZE+32)*4];
static spiffs sfs;

static const char tag[] = "spiffs_vfs";

static size_t vfs_write(void *ctx, int fd, const void *data, size_t size) {
  ESP_LOGI(tag, ">> write fd=%d, data=0x%lx, size=%d", fd, (unsigned long)data, size);
  spiffs *fs = (spiffs *)ctx;
  size_t retSize = SPIFFS_write(fs, (spiffs_file)fd, (void *)data, size);
  return retSize;
}

static off_t vfs_lseek(void *ctx, int fd, off_t offset, int whence) {
  ESP_LOGI(tag, ">> lseek fd=%d, offset=%d, whence=%d", fd, (int)offset, whence);
  return 0;
}

static ssize_t vfs_read(void *ctx, int fd, void *dst, size_t size) {
  ESP_LOGI(tag, ">> read fd=%d, dst=0x%lx, size=%d", fd, (unsigned long)dst, size);
  spiffs *fs = (spiffs *)ctx;
  ssize_t retSize = SPIFFS_read(fs, (spiffs_file)fd, dst, size);
  return retSize;
}

/**
 * Open the file specified by path. The flags contain the instructions on how
 * the file is to be opened. For example:
 *
 * O_CREAT  - Create the named file.
 * O_TRUNC  - Truncate (empty) the file.
 * O_RDONLY - Open the file for reading only.
 * O_WRONLY - Open the file for writing only.
 * O_RDWR   - Open the file for reading and writing.
 * O_APPEND - Append to the file.
 *
 * The mode are access mode flags.
 */
static int vfs_open(void *ctx, const char *path, int flags, int accessMode) {
  ESP_LOGI(tag, ">> open path=%s, flags=0x%x, accessMode=0x%x", path, flags, accessMode);
  spiffs *fs = (spiffs *)ctx;
  int spiffsFlags = SPIFFS_RDONLY;
//  if (flags & O_CREAT) {
//    spiffsFlags |= SPIFFS_CREAT;
//  }
//  if (flags & O_TRUNC) {
//    spiffsFlags |= SPIFFS_TRUNC;
//  }
//  if (flags & O_RDONLY) {
//    spiffsFlags |= SPIFFS_RDONLY;
//  }
//  if (flags & O_WRONLY) {
//    spiffsFlags |= SPIFFS_WRONLY;
//  }
//  if (flags & O_RDWR) {
//    spiffsFlags |= SPIFFS_RDWR;
//  }
//  if (flags & O_APPEND) {
//    spiffsFlags |= SPIFFS_APPEND;
//  }
  int rc = SPIFFS_open(fs, path, spiffsFlags, accessMode);
  ESP_LOGI(tag, ">> open path %s, rc=%d", path, rc);
  return rc;
}

static int vfs_close(void *ctx, int fd) {
  ESP_LOGI(tag, ">> close fd=%d", fd);
  spiffs *fs = (spiffs *)ctx;
  int rc = SPIFFS_close(fs, (spiffs_file)fd);
  return rc;
}

static int vfs_fstat(void *ctx, int fd, struct stat *st) {
  ESP_LOGI(tag, ">> fstat fd=%d", fd);
  spiffs_stat * ss = malloc(sizeof(spiffs_stat));
  int32_t rc = SPIFFS_fstat((spiffs *)ctx, (spiffs_file)fd, ss);

  if(rc == SPIFFS_OK) {
    st->st_size = ss->size;
    ESP_LOGI(tag, ">> size: %ld", st->st_size);
  }

  free(ss);

  return rc;
}

static int vfs_stat(void *ctx, const char *path, struct stat *st) {
  ESP_LOGI(tag, ">> stat path=%s", path);
  spiffs_stat * ss = malloc(sizeof(spiffs_stat));
  int32_t rc = SPIFFS_stat((spiffs *)ctx, path, ss);

  if(rc == SPIFFS_OK) {
    st->st_size = ss->size;
    ESP_LOGI(tag, ">> size: %ld", st->st_size);
  }

  free(ss);

  return rc;
}

static int vfs_link(void *ctx, const char *oldPath, const char *newPath) {
  ESP_LOGI(tag, ">> link oldPath=%s, newPath=%s", oldPath, newPath);
  return 0;
}

static int vfs_unlink(void *ctx, const char *path) {
  //ESP_LOGI(tag, ">> unlink path=%s", path);
  spiffs *fs = (spiffs *)ctx;
  SPIFFS_remove(fs, path);
  return 0;
}

static int vfs_rename(void *ctx, const char *oldPath, const char *newPath) {
  //ESP_LOGI(tag, ">> rename oldPath=%s, newPath=%s", oldPath, newPath);
  spiffs *fs = (spiffs *)ctx;
  int rc = SPIFFS_rename(fs, oldPath, newPath);
  return rc;
}

static int32_t partition_read(uint32_t addr, uint32_t size, uint8_t *dest)
{
  esp_err_t err;
  ESP_LOGD(tag, ">> partition read addr=%d, size=%d, dest=%p", addr, size, dest);

  err = esp_partition_read((esp_partition_t *)(sfs.user_data), addr, dest, size);

  if(err == ESP_OK) {
    return SPIFFS_OK;
  }

  ESP_LOGE(tag, ">> partition read err=%d", err);

  return err;
}

static int32_t partition_write(uint32_t addr, uint32_t size, uint8_t *src)
{
  esp_err_t err;
  ESP_LOGD(tag, ">> partition write addr=%d, size=%d, src=%p", addr, size, src);

  err = esp_partition_write((esp_partition_t *)(sfs.user_data), addr, src, size);

  if(err == ESP_OK) {
    return SPIFFS_OK;
  }

  ESP_LOGE(tag, ">> partition write err=%d", err);

  return err;
}

static int32_t partition_erase(uint32_t addr, uint32_t size)
{
  esp_err_t err;
  ESP_LOGD(tag, ">> partition erase addr=%d, size=%d", addr, size);

  err = esp_partition_erase_range((esp_partition_t *)(sfs.user_data), addr, size);

  if(err == ESP_OK) {
    return SPIFFS_OK;
  }

  ESP_LOGE(tag, ">> partition erase err=%d", err);

  return err;
}

/**
 * Register the VFS at the specified mount point. The callback functions are
 * registered to handle the different functions that may be requested against
 * the VFS.
 */
void spiffs_register_vfs(char *mount_point, const esp_partition_t *partition) {
  esp_vfs_t vfs;

  ESP_LOGI(tag, ">> registering partition at %x", partition->address);

  vfs.fd_offset = 0;
  vfs.flags = ESP_VFS_FLAG_CONTEXT_PTR;
  vfs.write_p  = vfs_write;
  vfs.lseek_p  = vfs_lseek;
  vfs.read_p   = vfs_read;
  vfs.open_p   = vfs_open;
  vfs.close_p  = vfs_close;
  vfs.fstat_p  = vfs_fstat;
  vfs.stat_p   = vfs_stat;
  vfs.link_p   = vfs_link;
  vfs.unlink_p = vfs_unlink;
  vfs.rename_p = vfs_rename;

  spiffs_config cfg;
  cfg.phys_size = partition->size;
  cfg.phys_addr = 0;
  cfg.phys_erase_block = 65536;
  cfg.log_block_size = 65536;
  cfg.log_page_size = LOG_PAGE_SIZE;

  cfg.hal_read_f =  partition_read;
  cfg.hal_write_f = partition_write;
  cfg.hal_erase_f = partition_erase;

  sfs.user_data = (esp_partition_t *)partition;

  int res = SPIFFS_mount(&sfs, &cfg, spiffs_work_buf, spiffs_fds,
    sizeof(spiffs_fds), spiffs_cache_buf, sizeof(spiffs_cache_buf), 0);

  if(res < 0) {
    ESP_LOGE(tag, ">> SPIFFS_mount failed: err=%d", res);
  }

  ESP_ERROR_CHECK(esp_vfs_register(mount_point, &vfs, &sfs));
}
