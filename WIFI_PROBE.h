#ifndef _WIFI_PROBE_H_
#define _WIFI_PROBE_H_

#define WIFI_PROBE_MAX_DEVICE  10   //最大记录的设备数
#define WIFI_PROBE_MAX_BUFF 64      //检测最长的buff
#define WIFI_PROBE_UPLOAD_CYCLE 180000     //同一MAC上报时间间隔   180秒

typedef struct _WIFI_PROBE_INFO
{
    unsigned long probe_time;  //上次一探测到的时间
    char device_mac[20];//探测到的设备的MAC地址
    unsigned char enable_flag; //使能标志
} WIFI_PROBE_INFO;

void WifiProbeInit();
void WifiProbeEvent();  // wifi探针任务
#endif
