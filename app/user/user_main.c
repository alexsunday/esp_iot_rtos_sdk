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
#define LOCAL_UDP_PORT 8090
#define LOCAL_SERVER_PORT 8091
#define CLOUD_SERVER "211.155.86.145"
#define CLOUD_PORT 10090
#define MAX_PACK_LENGTH 99
#define UDP_FINGERPRINT "I'm HERE, I'm iPLUG."


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


void rwinfo_init(rw_info* prw)
{
	memset(prw, 0, sizeof(rw_info));
	prw->server_addr = inet_addr(CLOUD_SERVER);
	prw->server_port = CLOUD_PORT;
	prw->dev_type = DEV_PLUG;
	strcpy(prw->ssid_mine, DEFAULT_SSID);
	strcpy(prw->ssid_pwd_mine, DEFAULT_SSID_PWD);
}


void ICACHE_FLASH_ATTR raw_show(unsigned char* buf, size_t buflen)
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

void ICACHE_FLASH_ATTR show_rw(rw_info* rw)
{
	raw_show((unsigned char*) rw, sizeof(rw_info));
	printf("Serv Addr: [%s]\n", inet_ntoa(rw->server_addr));
	printf("Serv Port: [%d]\n", rw->server_port);
	printf("Our SSID: [%s]\n", rw->ssid_mine);
	printf("Our SSID PWD: [%s]\n", rw->ssid_pwd_mine);
	printf("Router SSID: [%d],[%s]\n", strlen(rw->ssid), rw->ssid);
	printf("Router SSID PWD: [%s]\n", rw->ssid_pwd);
}


void ICACHE_FLASH_ATTR show_sysinfo()
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

uint8 ICACHE_FLASH_ATTR write_cfg_flash(rw_info* prw)
{
	//写入前，需要擦除
	if (spi_flash_erase_sector(FLASH_HEAD_ADDR / (4 * 1024))
			!= SPI_FLASH_RESULT_OK) {
		printf("SPI FLASH ERASE ERROR\n");
		return -1;
	}
	printf("SPI FLASH ERASE SUCCESS\n");

	//写入
	if (spi_flash_write(FLASH_HEAD_ADDR, (uint32*) prw, sizeof(rw_info))
			!= SPI_FLASH_RESULT_OK) {
		printf("SPI FLASH WRITE ERROR\n");
	}
	printf("SPI FLASH WRITE SUCCESS\n");

	return 0;
}


void ICACHE_FLASH_ATTR spi_flash_write_test(enum RUN_MODE mode)
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


void ICACHE_FLASH_ATTR spi_flash_read_test()
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


void ICACHE_FLASH_ATTR gpio_test(void* param)
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


void ICACHE_FLASH_ATTR client_mode(void* param)
{
	printf("CLIENT MODE\n");
	char rcvbuf[64];

	while(1) {
		int recvbytes, sin_size, str_len, sta_socket, iRet;
		struct sockaddr_in local_ip, remote_ip;
		memset(rcvbuf, 0, sizeof(rcvbuf));

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
		//设备报告
		rcvbuf[0] = 7;
		rcvbuf[1] = 0;
		int* pChipid = (int*)(rcvbuf + 2);
		*pChipid = system_get_chip_id();
		rcvbuf[6] = 1;
		send(sta_socket, rcvbuf, 7, 0);
		//循环读取

		while(1) {
			memset(rcvbuf, 0, sizeof(rcvbuf));
			iRet = recv(sta_socket, rcvbuf, 1, 0);
			if(iRet <= 0) {
				printf("[%d] disconnected...\n", sta_socket);
				close(sta_socket);
				break;
			}

			if(rcvbuf[0] > MAX_PACK_LENGTH) {
				printf("[%d] conn protocol error, max pack length overload\n", sta_socket);
				close(sta_socket);
				break;
			}

			iRet = recv(sta_socket, rcvbuf + 1, rcvbuf[0] - 1, 0);
			if(iRet <= 0) {
				printf("[%d] disconnected...\n", sta_socket);
				close(sta_socket);
				break;
			}

			switch(rcvbuf[1])
			{
			case 1:
				//HEARTBEAT
				printf("Heart beat req\n");
				rcvbuf[0] = 2;
				rcvbuf[1] = 5;
				send(sta_socket, rcvbuf, 2, 0);
				break;
			case 4:
				//dev rp rsp
				printf("Dev RP RSP\n");
				break;
			case 8:
			{
				//LED TEST
				printf("RECV LED TEST\n");
				rcvbuf[0] = 2;
				rcvbuf[1] = 9;
				send(sta_socket, rcvbuf, 2, 0);
				break;
			}
			case 12:
			{
				//RST
				printf("REST \n");
				rcvbuf[0] = 2;
				rcvbuf[1] = 13;
				send(sta_socket, rcvbuf, 2, 0);
				vTaskDelay(50 / portTICK_RATE_MS);
				close(sta_socket);
				system_restart();
				break;
			}
			default:
				break;
			}

			printf("one pack proc over.\n");
		}

		if(recvbytes <= 0) {
			printf("disconnected...\n");
		}

		vTaskDelay(10 / portTICK_RATE_MS);
	}

	vTaskDelete(NULL);
}


void ICACHE_FLASH_ATTR wifi_mode_client_conn(void* param)
{
	int left_bytes, has_recv;
	int sock = (int)param;
	char rcvbuf[100] = { 0 };
	char* pcontent = NULL;

	printf("wifi_mode_client_conn, sock: [%d]\n", sock);
	while(1) {
		printf("while ... while\n");
		has_recv = recv(sock, rcvbuf, 1, 0);
		printf("recv...recv\n");
		if(has_recv <= 0) {
			printf("disconnected...\n");
			close(sock);
			break;
		}
		printf("recv tcp head OK\n");

		if(rcvbuf[0] > MAX_PACK_LENGTH) {
			printf("head length large than %d\n", MAX_PACK_LENGTH);
			close(sock);
			break;
		}
		printf("tcp head length OK\n");

		pcontent = rcvbuf + 1;
		left_bytes = rcvbuf[0] - 1;
		has_recv = 0;

		printf("will recv while\n");
		while(left_bytes) {
			has_recv = recv(sock, pcontent + has_recv, left_bytes, 0);
			if(has_recv <= 0) {
				printf("disconnected...\n");
				break;
			}
			left_bytes -= has_recv;
		}

		if(has_recv <= 0) {
			close(sock);
			break;
		}

		printf("data pack judge\n");
		switch(pcontent[0])
		{
		case 1:
			//HEARTBEAT
			printf("Heart beat req\n");
			rcvbuf[0] = 2;
			rcvbuf[1] = 5;
			send(sock, rcvbuf, 2, 0);
			break;
		case 8:
			//LED TEST
			printf("RECV LED TEST\n");
			rcvbuf[0] = 2;
			rcvbuf[1] = 9;
			send(sock, rcvbuf, 2, 0);
			break;
		case 10:
			//SET SSID
			break;
		default:
			break;
		}

		vTaskDelay(10 / portTICK_RATE_MS);
	}

	vTaskDelete(NULL);
}


void ICACHE_FLASH_ATTR wifi_mode(void* param)
{
	rw_info rw;
	char rcvbuf[64];
	char* ssid = NULL;
	char* ssid_pwd = NULL;

	rwinfo_init(&rw);

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
			printf("socket listen ok\n");
			sin_size = sizeof(client_addr);

			for(;;) {
				client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &sin_size);
				if(client_sock < 0) {
					printf("accept error\n");
					continue;
				}
				printf("New connection from sock:[%d], addr: [%s:%d]\n",
						client_sock, inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));

				while(1) {
					memset(rcvbuf, 0, sizeof(rcvbuf));
					iRet = recv(client_sock, rcvbuf, 1, 0);
					if(iRet <= 0) {
						printf("[%d] disconnected...\n", client_sock);
						close(client_sock);
						break;
					}

					if(rcvbuf[0] > MAX_PACK_LENGTH) {
						printf("[%d] conn protocol error, max pack length overload\n", client_sock);
						close(client_sock);
						break;
					}

					iRet = recv(client_sock, rcvbuf + 1, rcvbuf[0] - 1, 0);
					if(iRet <= 0) {
						printf("[%d] disconnected...\n", client_sock);
						close(client_sock);
						break;
					}

					switch(rcvbuf[1])
					{
					case 1:
						//HEARTBEAT
						printf("Heart beat req\n");
						rcvbuf[0] = 2;
						rcvbuf[1] = 5;
						send(client_sock, rcvbuf, 2, 0);
						break;
					case 8:
					{
						//LED TEST
						printf("RECV LED TEST\n");
						rcvbuf[0] = 2;
						rcvbuf[1] = 9;
						send(client_sock, rcvbuf, 2, 0);
						break;
					}
					case 10:
					{
						//SET SSID
						printf("set ssid req\n");
						ssid = rcvbuf + 2;
						if(strlen(ssid) >= 32 || strlen(ssid) <= 0) {
							printf("ssid large than 32 byte.\n");
							close(client_sock);
							break;
						}

						ssid_pwd = rcvbuf + 2 + strlen(ssid) + 1;
						if(strlen(ssid_pwd) >= 32 || strlen(ssid_pwd) <= 0) {
							printf("ssid_pwd large than 32 byte.\n");
							close(client_sock);
							break;
						}

						strcpy(rw.ssid, ssid);
						strcpy(rw.ssid_pwd, ssid_pwd);
						rw.run_mode = CLIENT_ONLY;
						if(write_cfg_flash(&rw) < 0) {
							printf("write_cfg_flash error\n");
						}

						rcvbuf[0] = 2;
						rcvbuf[1] = 11;
						send(client_sock, rcvbuf, 2, 0);

						break;
					}
					case 12:
					{
						//RST
						printf("REST \n");
						rcvbuf[0] = 2;
						rcvbuf[1] = 13;
						send(client_sock, rcvbuf, 2, 0);
						vTaskDelay(50 / portTICK_RATE_MS);
						close(client_sock);
						system_restart();
						break;
					}
					default:
						break;
					}

					printf("one pack proc over.\n");
				}
				//xTaskCreate(wifi_mode_client_conn, "wifi_mode_client_conn", 128, (void*)client_sock, 0, NULL);
			}

		} while(0);

		vTaskDelay(1000 / portTICK_RATE_MS);
	}

	vTaskDelete(NULL);
}


void ICACHE_FLASH_ATTR udp_boardcast(void* param)
{
	while(1) {
		int opt = 1;
		int iRet = 0;
		int sock = 0;
		struct sockaddr_in addrto, addrfrom;

		memset(&addrto, 0, sizeof(struct sockaddr_in));
		memset(&addrfrom, 0, sizeof(struct sockaddr_in));

		addrto.sin_family = AF_INET;
		addrto.sin_addr.s_addr = htonl(INADDR_ANY);
		addrto.sin_port = htons(LOCAL_UDP_PORT);

		addrfrom.sin_family = AF_INET;
		addrfrom.sin_addr.s_addr = htonl(INADDR_ANY);
		addrfrom.sin_port = htons(LOCAL_UDP_PORT);

		do {
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			if(sock == -1) {
				printf("udp socket init error\n");
				break;
			}
			printf("udp socket init ok\n");

			iRet = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));
			if(iRet == -1) {
				printf("udp setsockopt error\n");
				break;
			}
			printf("udp set socket opt ok\n");

			iRet = bind(sock, (struct sockaddr*)&addrto, sizeof(struct sockaddr_in));
			if(iRet == -1) {
				printf("udp bind error\n");
				break;
			}
			printf("udp socket bind ok\n");

			char rcvbuf[16] = { 0 };
			int sock_len = sizeof(struct sockaddr_in);
			while(1) {
				int recvbytes = recvfrom(sock, rcvbuf, sizeof(rcvbuf), 0, (struct sockaddr*)&addrfrom, (socklen_t*)&sock_len);
				if(recvbytes <= 0) {
					printf("udp recvfrom error\n");
				} else {
					if(recvbytes >= sizeof(rcvbuf)) {
						rcvbuf[sizeof(rcvbuf) - 1] = 0;
					} else {
						rcvbuf[recvbytes] = 0;
					}
					printf("udp recv:[%d],[%s]\n", recvbytes, rcvbuf);
					printf("recv from: [%s:%d]\n", inet_ntoa(addrfrom.sin_addr), htons(addrfrom.sin_port));

					printf("I'm HERE\n");
					sock_len = sizeof(addrfrom);
					sendto(sock, UDP_FINGERPRINT, strlen(UDP_FINGERPRINT), 0, (struct sockaddr*)&addrfrom, sock_len);
				}

				vTaskDelay(10 / portTICK_RATE_MS);
			}
		} while(0);

		close(sock);
		vTaskDelay(10 / portTICK_RATE_MS);
	}

	vTaskDelete(NULL);
}


void ICACHE_FLASH_ATTR user_init(void)
{
	uint8 iRet;
	rw_info* prw = (rw_info*)zalloc(sizeof(rw_info));

	uart_init_new();
	//spi_flash_write_test(WIFI_BOARDCAST);
	show_sysinfo();

	if (spi_flash_read(FLASH_HEAD_ADDR, (uint32*) prw, sizeof(rw_info))
			!= SPI_FLASH_RESULT_OK) {
		printf("spi_flash_read error\n");
		iRet = -1;
	} else {
		printf("spi_flash_read success\n");
		iRet = 0;
	}
	show_rw(prw);
	xTaskCreate(gpio_test, "gpio_led", 256, (void*)prw, 0, NULL);

	/* need to set opmode before you set config */

	if(prw->run_mode == CLIENT_ONLY)
    {
		wifi_set_opmode(STATION_MODE);
        struct station_config *config = (struct station_config *)zalloc(sizeof(struct station_config));
        sprintf(config->ssid, prw->ssid);
        sprintf(config->password, prw->ssid_pwd);

        /* need to sure that you are in station mode first,
         * otherwise it will be failed. */
        wifi_station_set_config(config);
        free(config);
        xTaskCreate(client_mode, "client_mode", 256, NULL, 0, NULL);
		xTaskCreate(udp_boardcast, "udp_boardcast", 256, NULL, 0, NULL);
		xTaskCreate(wifi_mode, "wifi_mode", 256, NULL, 0, NULL);
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
}
