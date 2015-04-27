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
#include "lwip/sockets.h"

#include "gpio.h"

#define FLASH_HEAD_ADDR 0x3C000
#define DEFAULT_SSID "iPLUG"
#define DEFAULT_SSID_PWD "iPLUG123"
#define DEFAULT_GWADDR "192.168.10.253"
#define DHCP_BEGIN_ADDR "192.168.10.100"
#define DHCP_END_ADDR "192.168.10.110"
#define LOCAL_SERVER_PORT 8091
#define CLOUD_SERVER "211.155.86.145"
#define CLOUD_PORT 10090


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


void spi_flash_write_test(enum RUN_MODE mode)
{
	rw_info rw;
	memset(&rw, 0, sizeof(rw));
	rw.server_addr = inet_addr("192.168.0.241");
	rw.server_port = 10010;
	strcpy(rw.ssid, "useease2");
	strcpy(rw.ssid_pwd, "1CBE991A14");
	strcpy(rw.ssid_mine, "iPLUG");
	strcpy(rw.ssid_pwd_mine, "iPLUG123");
	rw.run_mode = mode;
	rw.dev_type = DEV_PLUG;

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
	int pin = 0;

	rw_info* rw = (rw_info*)param;

	if(rw->run_mode == CLIENT_ONLY) {
		pin = BIT12;
	} else if(rw->run_mode == WIFI_BOARDCAST) {
		pin = BIT13;
	} else {
		pin = BIT14;
	}

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U,FUNC_GPIO14);

	while (1) {
		gpio_output_set(pin, 0, pin, 0);
		vTaskDelay(1000 / portTICK_RATE_MS);
		gpio_output_set(0, pin, pin, 0);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}


void client_mode(void* param)
{
	printf("CLIENT MODE\n");

	while(1) {
		int recvbytes, sin_size, str_len, sta_socket, iRet;
		struct sockaddr_in local_ip, remote_ip;

		sta_socket = socket(AF_INET, SOCK_STREAM, 0);
		if(-1 == sta_socket) {
			printf("sock init error\n");
			close(sta_socket);
			continue;
		}
		printf("client socket init ok.\n");

		memset(&remote_ip, 0, sizeof(struct sockaddr_in));
		remote_ip.sin_family = AF_INET;
		remote_ip.sin_addr.s_addr = inet_addr(CLOUD_SERVER);
		remote_ip.sin_port = htons(CLOUD_PORT);

		iRet = connect(sta_socket, (struct sockaddr*)&remote_ip, sizeof(struct sockaddr));
		if(iRet != 0) {
			printf("Connect fail\n");
			close(sta_socket);
			vTaskDelay(5000 / portTICK_RATE_MS);
			continue;
		}
		printf("Connected CLOUD :[%s:%d]\n", CLOUD_SERVER, CLOUD_PORT);
		char* recv_buf = (char*)zalloc(128);
		while((recvbytes = read(sta_socket, recv_buf, 128)) > 0) {
			recv_buf[recvbytes] = 0;
			printf("Data from CLIENT: [%d], [%s]\n", recvbytes, recv_buf);
		}
		free(recv_buf);

		if(recvbytes <= 0) {
			printf("disconnected...\n");
		}

		vTaskDelay(10 / portTICK_RATE_MS);
	}

	vTaskDelete(NULL);
}


void wifi_mode(void* param)
{
	while(1) {
		struct sockaddr_in server_addr, client_addr;
		int server_sock, client_sock, iRet;
		socklen_t sin_size;

		memset(&server_addr, 0, sizeof(struct sockaddr_in));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = INADDR_ANY;
		server_addr.sin_port = htons(LOCAL_SERVER_PORT);
		int recvbytes;

		do {
			server_sock = socket(AF_INET, SOCK_STREAM, 0);
			if(server_sock == -1) {
				printf("socket error\n");
				break;
			}
			printf("init socket ok\n");

			iRet = bind(server_sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
			if(iRet == -1) {
				printf("BIND ERROR\n");
				break;
			}
			printf("socket bind ok\n");

			iRet = listen(server_sock, 2);
			if(iRet == -1) {
				printf("listen error\n");
				break;
			}
			printf("socket listen ok");
			sin_size = sizeof(client_addr);

			for(;;) {
				client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &sin_size);
				if(client_sock < 0) {
					printf("accept error\n");
					continue;
				}

				printf("New connection from [%s:%d]\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));
				char* recv_buf = (char*)zalloc(128);
				while((recvbytes = read(client_sock, recv_buf, 128)) > 0) {
					recv_buf[recvbytes] = 0;
					printf("RECV: [%d], [%s]\n", recvbytes, recv_buf);
				}
				free(recv_buf);

				if(recvbytes <= 0) {
					printf("disconnected...\n");
					close(client_sock);
				}
			}

		} while(0);

		vTaskDelay(1000 / portTICK_RATE_MS);
	}

	vTaskDelete(NULL);
}


void ICACHE_FLASH_ATTR user_init(void)
{
	uint8 iRet;
	rw_info rw;

	uart_init_new();
	//spi_flash_write_test(WIFI_BOARDCAST);
    printf("SDK version:%s\n", system_get_sdk_version());

	if (spi_flash_read(FLASH_HEAD_ADDR, (uint32*) &rw, sizeof(rw))
			!= SPI_FLASH_RESULT_OK) {
		printf("spi_flash_read error\n");
		iRet = -1;
	} else {
		printf("spi_flash_read success\n");
		iRet = 0;
	}

	/* need to set opmode before you set config */

	if(rw.run_mode == CLIENT_ONLY)
    {
		wifi_set_opmode(STATION_MODE);
        struct station_config *config = (struct station_config *)zalloc(sizeof(struct station_config));
        sprintf(config->ssid, "useease2");
        sprintf(config->password, "1CBE991A14");

        /* need to sure that you are in station mode first,
         * otherwise it will be failed. */
        wifi_station_set_config(config);
        free(config);
        xTaskCreate(client_mode, "client_mode", 256, NULL, 0, NULL);
    }
	else
	{
		wifi_set_opmode(SOFTAP_MODE);
		struct softap_config apconfig;
		memset(&apconfig, 0, sizeof(struct softap_config));
		strcpy(apconfig.ssid, DEFAULT_SSID);
		strcpy(apconfig.password, DEFAULT_SSID_PWD);
		apconfig.ssid_len = 0;
		apconfig.authmode = AUTH_WPA_WPA2_PSK;
		apconfig.ssid_hidden = 0;
		apconfig.max_connection = 5;
		apconfig.beacon_interval = 100;

		if (!wifi_softap_set_config(&apconfig)) {
			printf("[%s] [%s] ERROR\n", __func__,
					"wifi_softap_set_config");
		}
		printf("wifi_softap_set_config success\n");

		struct ip_info ipinfo;

        ipinfo.gw.addr = ipaddr_addr(DEFAULT_GWADDR);
    	ipinfo.ip.addr = ipaddr_addr(DEFAULT_GWADDR);
    	ipinfo.netmask.addr = ipaddr_addr("255.255.255.0");

    	wifi_set_ip_info(SOFTAP_IF, &ipinfo);

    	struct dhcp_info *pdhcp_info = NULL;

        pdhcp_info = (struct dhcp_info *)zalloc(sizeof(struct dhcp_info));
        pdhcp_info->start_ip = ipaddr_addr(DHCP_BEGIN_ADDR);
        pdhcp_info->end_ip = ipaddr_addr(DHCP_END_ADDR);    // don't set the range too large, because it will cost memory.
        pdhcp_info->max_leases = 10;
        pdhcp_info->auto_time = 60;
        pdhcp_info->decline_time = 60;
        pdhcp_info->conflict_time = 60;
        pdhcp_info->offer_time = 60;
        pdhcp_info->min_lease_sec = 60;
        dhcp_set_info(pdhcp_info);
        free(pdhcp_info);
		udhcpd_start();

		xTaskCreate(wifi_mode, "wifi_mode", 256, NULL, 0, NULL);
	}

	xTaskCreate(gpio_test, "gpio_led", 256, (void*)&rw, 0, NULL);
}
