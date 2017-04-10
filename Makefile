#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := webserver

CPPFLAGS = -D RTOS_SDK -D MG_DISABLE_MQTT -D MG_DISABLE_COAP -D MG_ENABLE_HTTP_WEBSOCKET -D MG_ENABLE_FILESYSTEM

include $(IDF_PATH)/make/project.mk

read_fs:
	$(ESPTOOLPY_SERIAL) read_flash 0x110000 0x100000 flash.img

write_fs:
	mkspiffs -c fs -b 65536 -p 256 -s 1048576 fs.img
	$(ESPTOOLPY_WRITE_FLASH) 0x110000 fs.img

erase_fs:
	$(ESPTOOLPY_SERIAL) erase_region 0x110000 0x100000
