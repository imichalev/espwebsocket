#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := app-template
#COMPONENTS=freertos esp32 newlib esptool_py mbedtls soc wpa_supplicant heap vfs nvs_flash spi_flash log tcpip_adapter lwip main xtensa-debug-module driver 
include $(IDF_PATH)/make/project.mk

