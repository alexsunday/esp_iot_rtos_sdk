/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/12/1, v1.0 create this file.
 *******************************************************************************/
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "udhcp/dhcpd.h"
#include "lwip/inet.h"

#include "gpio.h"

#define FLASH_HEAD_ADDR 0x3C000
#define DEFAULT_SSID "iPLUG"
#define DEFAULT_SSID_PWD "iPLUG123"
#define DEFAULT_GWADDR "192.168.10.1"
#define DHCP_BEGIN_ADDR "192.168.10.101"
#define DHCP_END_ADDR "192.168.10.108"

enum DEV_TYPE {
	DEV_UNKNOWN, DEV_PLUG
};

enum RUN_MODE {
	MODE_UNKNOWN, WIFI_BOARDCAST, CLIENT_ONLY
};

typedef struct _rw_info{
	uint32 server_addr;
	uint16 server_port;
	unsigned char ssid_mine[32];
	unsigned char ssid_pwd_mine[16];
	unsigned char ssid[32];
	unsigned char ssid_pwd[16];
	uint8 run_mode;
	uint8 dev_type;
} rw_info;

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR user_init_(void)
{
	uart_init_new();

	/* need to set opmode before you set config */
	wifi_set_opmode(STATIONAP_MODE);

	{
		struct station_config *config = (struct station_config *) zalloc(
				sizeof(struct station_config));
		sprintf(config->ssid, "ChinaNet-966");
		sprintf(config->password, "Jchen0406");

		/* need to sure that you are in station mode first,
		 * otherwise it will be failed. */
		wifi_station_set_config(config);
		free(config);
	}

	{
		struct ip_info ipinfo;

		ipinfo.gw.addr = ipaddr_addr("192.168.145.253");
		ipinfo.ip.addr = ipaddr_addr("192.168.145.253");
		ipinfo.netmask.addr = ipaddr_addr("255.255.255.0");

		wifi_set_ip_info(SOFTAP_IF, &ipinfo);
	}

//    {
//        struct dhcp_info *pdhcp_info = NULL;
//
//        pdhcp_info = (struct dhcp_info *)zalloc(sizeof(struct dhcp_info));
//        pdhcp_info->start_ip = ipaddr_addr("192.168.145.100");
//        pdhcp_info->end_ip = ipaddr_addr("192.168.145.110");    // don't set the range too large, because it will cost memory.
//        pdhcp_info->max_leases = 10;
//        pdhcp_info->auto_time = 60;
//        pdhcp_info->decline_time = 60;
//        pdhcp_info->conflict_time = 60;
//        pdhcp_info->offer_time = 60;
//        pdhcp_info->min_lease_sec = 60;
//        dhcp_set_info(pdhcp_info);
//        free(pdhcp_info);
//    }
//
//    udhcpd_start();

//xTaskCreate(test_select, "test_select", 256, NULL, 2, NULL);
//xTaskCreate(task2, "tsk2", 256, NULL, 2, NULL);
//xTaskCreate(task3, "tsk3", 256, NULL, 2, NULL);
}

void raw_show(unsigned char* buf, size_t buflen)
{
	int i = 0;
	printf("*************************RAW_INFO*************************\n");
	for (; i != buflen; ++i) {
		if (i % 16 == 0) {
			printf("\n");
		}
		printf("%02X ", buf[i]);
	}

	printf("\n");
	printf("*************************RAW_INFO*************************\n");
}

void show_rw(rw_info* rw)
{
	raw_show((unsigned char*) rw, sizeof(rw_info));
	printf("Serv Addr: [%s]\n", inet_ntoa(rw->server_addr));
	printf("Serv Port: [%d]\n", rw->server_port);
	printf("Our SSID: [%s]\n", rw->ssid_mine);
	printf("Our SSID PWD: [%s]\n", rw->ssid_pwd_mine);
	printf("Router SSID: [%d],[%s]\n", strlen(rw->ssid), rw->ssid);
	printf("Router SSID PWD: [%s]\n", rw->ssid_pwd);
}

void show_sysinfo()
{
	uint32 chipid = system_get_chip_id();
	uint32 heepsize = system_get_free_heap_size();
	uint32 rtctime = system_get_rtc_time();
	uint32 systime = system_get_time();

	printf("SDK version: [%s]\n", system_get_sdk_version());

	printf("SYSTEM INIT OVER\n");
	printf("==========SYS INFO==========\n");
	system_print_meminfo();
	printf("CHIP   ID: [%d]\n", chipid);
	printf("HEAP SIZE: [%d]\n", heepsize);
	printf("RTC  TIME: [%d]\n", rtctime);
	printf("SYS  TIME: [%d]\n", systime);
	printf("==========SYS INFO==========\n");
}

void spi_flash_write_test()
{
	rw_info rw;
	memset(&rw, 0, sizeof(rw));
	rw.server_addr = inet_addr("192.168.10.1");
	rw.server_port = 10010;
	strcpy(rw.ssid, "ChinaNet-966");
	strcpy(rw.ssid_pwd, "Jchen0406");
	strcpy(rw.ssid_mine, "iPLUG");
	strcpy(rw.ssid_pwd_mine, "iPLUG123");

	show_rw(&rw);

	//写入前，需要擦除
	if (spi_flash_erase_sector(FLASH_HEAD_ADDR / (4 * 1024))
			!= SPI_FLASH_RESULT_OK) {
		printf("SPI FLASH ERASE ERROR\n");
	} else {
		printf("SPI FLASH ERASE SUCCESS\n");
	}
	//写入
	if (spi_flash_write(FLASH_HEAD_ADDR, (uint32*) &rw, sizeof(rw))
			!= SPI_FLASH_RESULT_OK) {
		printf("SPI FLASH WRITE ERROR\n");
	} else {
		printf("SPI FLASH WRITE SUCCESS\n");
	}
}

void spi_flash_read_test()
{
	rw_info rw;
	memset(&rw, 0, sizeof(rw));

	if (spi_flash_read(FLASH_HEAD_ADDR, (uint32*) &rw, sizeof(rw))
			!= SPI_FLASH_RESULT_OK) {
		printf("FLASH READ ERROR\n");
	} else {
		printf("FLASH READ SUCCESS\n");
		show_rw(&rw);
	}
}

void gpio_test(void* param)
{
	int pin = BIT13;
	//PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	printf("========================GPIO TEST========================\n");
	while (1) {
		printf("LED UP\n");
		gpio_output_set(pin, 0, pin, 0);
		vTaskDelay(1000 / portTICK_RATE_MS);
		printf("LED DOWN\n");
		gpio_output_set(0, pin, pin, 0);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	printf("========================GPIO TEST========================\n");
}


void local_server(void* param)
{
	while(1)
	{
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}


uint8 wifi_boardcast_mode(rw_info* rw)
{
	if (!wifi_set_opmode_current(SOFTAP_MODE)) {
		printf("[%s] [%s] ERROR\n", "wifi_set_opmode", __func__);
		return -1;
	}

	{
		struct softap_config apconfig;
		memset(&apconfig, 0, sizeof(struct softap_config));
		strcpy(apconfig.ssid, DEFAULT_SSID);
		strcpy(apconfig.password, DEFAULT_SSID_PWD);
		apconfig.ssid_len = 0;
		apconfig.authmode = AUTH_WPA_WPA2_PSK;
		apconfig.ssid_hidden = 0;
		apconfig.max_connection = 5;
		apconfig.beacon_interval = 100;

		if (!wifi_softap_set_config_current(&apconfig)) {
			printf("[%s] [%s] ERROR\n", __func__,
					"wifi_softap_set_config_current");
			return -1;
		}
	}

	{
		struct ip_info ipinfo;

		ipinfo.gw.addr = ipaddr_addr(DEFAULT_GWADDR);
		ipinfo.ip.addr = ipaddr_addr(DEFAULT_GWADDR);
		ipinfo.netmask.addr = ipaddr_addr("255.255.255.0");

		if(!wifi_set_ip_info(SOFTAP_IF, &ipinfo)) {
			printf("[%s] [%s] ERROR\n", __func__, "wifi_set_ip_info ap_if");
			return -1;
		}
	}

	{
		struct dhcp_info dhcpinfo;

		dhcpinfo.start_ip = ipaddr_addr(DHCP_BEGIN_ADDR);
		dhcpinfo.end_ip = ipaddr_addr(DHCP_END_ADDR);
		dhcpinfo.max_leases = 10;
		dhcpinfo.auto_time = 60;
		dhcpinfo.decline_time = 60;
		dhcpinfo.conflict_time = 60;
		dhcpinfo.offer_time = 60;
		dhcpinfo.min_lease_sec = 60;
		dhcp_set_info(&dhcpinfo);
		udhcpd_start();
	}

	xTaskCreate(local_server, "local_server", 256, NULL, 0, NULL);

	return 0;
}

uint8 client_only_mode(rw_info* rw)
{
	if (!wifi_set_opmode_current(STATION_MODE)) {
		printf("[%s] [%s] ERROR\n", "wifi_set_opmode", __func__);
		return -1;
	}

	{
		//连接AP
		struct station_config config;
		struct ip_info ipinfo;
		bool status;

		strcpy(config.ssid, rw->ssid);
		strcpy(config.password, rw->ssid_pwd);
		status = wifi_station_set_config_current(&config);
		if (!status) {
			printf("[%s] [%s] Config fail\n", rw->ssid, rw->ssid_pwd);
			return -1;
		}

		if (!wifi_station_connect()) {
			printf("[%s] WIFI CONNECT AP ERROR\n", rw->ssid);
			return -1;
		} else {
			printf("[%s] WIFI CONNECTED!\n", rw->ssid);
		}

		//连接成功后，开启DHCPC
		if (!wifi_station_dhcpc_start()) {
			printf("[%s] DHCPC start ERROR\n", rw->ssid);
			return -1;
		} else {
			printf("[%s] DHCP CLIENT START OK\n", rw->ssid);
		}

		//显示IP地址
		if (!wifi_get_ip_info(STATION_IF, &ipinfo)) {
			printf("[%s] [%s] ERROR\n", rw->ssid, __func__);
			return -1;
		}

		printf("IP ADDR: [%s]\n", inet_ntoa(ipinfo.ip.addr));
		printf("NETMASK: [%s]\n", inet_ntoa(ipinfo.netmask.addr));
		printf("GW ADDR: [%s]\n", inet_ntoa(ipinfo.gw.addr));
	}

	{
		//连接云端
		printf("ALL OK, prepare to connect CLOUD\n");
	}

	return 0;
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uint8 iRet;
	rw_info rw;

	uart_init_new();
	vTaskDelay(5000 / portTICK_RATE_MS);

	show_sysinfo();

	if (spi_flash_read(FLASH_HEAD_ADDR, (uint32*) &rw, sizeof(rw))
			!= SPI_FLASH_RESULT_OK) {
		printf("spi_flash_read error\n");
		iRet = -1;
	} else {
		printf("spi_flash_read success\n");
		iRet = 0;
	}

	if (iRet == 0 && rw.run_mode == CLIENT_ONLY) {
		printf("CLIENT MODE\n");
		client_only_mode(&rw);
	} else {
		printf("WIFI BOARDCAST MODE\n");
		wifi_boardcast_mode(&rw);
	}
}
