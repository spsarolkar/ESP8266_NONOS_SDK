/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "sntp.h"
#include "uart.h"
#include <string.h>

MQTT_Client mqttClient;
typedef unsigned long u32_t;
static ETSTimer sntp_timer;

void sntpfn()
{
    u32_t ts = 0;
    ts = sntp_get_current_timestamp();
    os_printf("current time : %s\n", sntp_get_real_time(ts));
    if (ts == 0) {
        //os_printf("did not get a valid time from sntp server\n");
    } else {
            os_timer_disarm(&sntp_timer);
            MQTT_Connect(&mqttClient);
    }
}

void wifiConnectCb(uint8_t status)
{
    if(status == STATION_GOT_IP){
        sntp_setservername(0, "pool.ntp.org");        // set sntp server after got ip address
        sntp_init();
        os_timer_disarm(&sntp_timer);
        os_timer_setfn(&sntp_timer, (os_timer_func_t *)sntpfn, NULL);
        os_timer_arm(&sntp_timer, 1000, 1);//1s
    } else {
          MQTT_Disconnect(&mqttClient);
    }
}

void mqttConnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Connected\r\n");
    char *data = "{\"name\": \"bedroom\", \"command_topic\": \"homeassistant/switch/bedroom/set\", \"payload_on\": \"ON\", \"payload_off\": \"OFF\", \"availability_topic\": \"homeassistant/switch/bedroom/available\", \"state_topic\": \"homeassistant/bedroom/state\"}";
    MQTT_Publish(client, "homeassistant/switch/bedroom/config", data, strlen(data), 0, 0);
    MQTT_Subscribe(client, "homeassistant/switch/bedroom/set", 0);
    char *status = "online";
    MQTT_Publish(client, "homeassistant/switch/bedroom/available", status, strlen(status), 0, 0);
    /* MQTT_Subscribe(client, "/mqtt/topic/0", 0);
    MQTT_Subscribe(client, "/mqtt/topic/1", 1);
    MQTT_Subscribe(client, "/mqtt/topic/2", 2);

    MQTT_Publish(client, "/mqtt/topic/0", "hello0", 6, 0, 0);
    MQTT_Publish(client, "/mqtt/topic/1", "hello1", 6, 1, 0);
    MQTT_Publish(client, "/mqtt/topic/2", "hello2", 6, 2, 0);
*/
}

void mqttDisconnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    char *topicBuf = (char*)os_zalloc(topic_len+1),
            *dataBuf = (char*)os_zalloc(data_len+1);

    MQTT_Client* client = (MQTT_Client*)args;

    os_memcpy(topicBuf, topic, topic_len);
    topicBuf[topic_len] = 0;

    os_memcpy(dataBuf, data, data_len);
    dataBuf[data_len] = 0;

    INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

    char *on_state = "ON";
    char *off_state = "OFF";
    INFO("checking %s,%s vs %s\n",on_state,off_state,dataBuf);
    if(strcmp(on_state, dataBuf)==0) {

        INFO("Publishing on state\n");
        MQTT_Publish(client, "homeassistant/bedroom/state", on_state, strlen(on_state), 0, 0);
        INFO("Sending UART\n");
        uart0_sendStr("1-1\r\n");
        INFO("UART sent\n");
    }
    else  if(strcmp(off_state, dataBuf)==0){
        INFO("Publishing off state");
        MQTT_Publish(client, "homeassistant/bedroom/state", off_state, strlen(off_state), 0, 0);
        INFO("Sending UART\n");
        uart0_sendStr("1-0\r\n");
        INFO("UART sent\n");


    }


    os_free(topicBuf);
    os_free(dataBuf);
}


/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}


void user_init(void)
{
    uart_init(BIT_RATE_9600, BIT_RATE_115200);

    os_delay_us(60000);
    system_uart_swap();
    CFG_Load();

    MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
    //MQTT_InitConnection(&mqttClient, "192.168.11.122", 1880, 0);

    MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
    //MQTT_InitClient(&mqttClient, "client_id", "user", "pass", 120, 1);

    MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
    MQTT_OnConnected(&mqttClient, mqttConnectedCb);
    MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
    MQTT_OnPublished(&mqttClient, mqttPublishedCb);
    MQTT_OnData(&mqttClient, mqttDataCb);

    WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

    INFO("\r\nSystem started ...\r\n");
}
void uart_recvTask(os_event_t *events)
{
INFO("Received");
char receivedStr[100]="";
if(events->sig == 0){
#if  UART_BUFF_EN
Uart_rx_buff_enq();
#else
uint8 fifo_len = (READ_PERI_REG(UART_STATUS(UART0))>>UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
char d_tmp = 0;
uint8 idx=0;
for(idx=0;idx<fifo_len;idx++) {
d_tmp = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
    receivedStr[idx]=d_tmp;
    //INFO("%c\r\n",d_tmp);
}
    //receivedStr[idx++]='\n';
    //receivedStr[idx++]='\n';
   // MQTT_Publish(&mqttClient, "homeassistant/bedroom/state", receivedStr, strlen(receivedStr), 0, 0);
    receivedStr[fifo_len-2]='\0';
    char *on_state = "ON";
    char *off_state = "OFF";
    INFO("-%s- vs -%s-%d-%d-%d\r\n",receivedStr,"1-1",fifo_len,strlen(receivedStr),strcmp(receivedStr,"1-1"));
    if(strcmp(receivedStr,"1-1")==0){
        MQTT_Publish(&mqttClient, "homeassistant/bedroom/state", on_state, strlen(on_state), 0, 0);

    }else if(strcmp(receivedStr,"1-0")==0){
        MQTT_Publish(&mqttClient, "homeassistant/bedroom/state", off_state, strlen(off_state), 0, 0);

    }

    //receivedStr[idx++]='\n';
    INFO("%s\r\n",receivedStr);

WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR);
uart_rx_intr_enable(UART0);
#endif
}else if(events->sig == 1){
#if UART_BUFF_EN
//already move uart buffer output to uart empty interrupt
        //tx_start_uart_buffer(UART0);
#else

#endif
}
}