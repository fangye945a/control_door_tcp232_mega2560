#ifndef _COMMON_H_
#define _COMMON_H_
#include "arduino.h"
#include <avr/wdt.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <SD.h>
#define DE_BUG
#define GSM_UART Serial1
#define RFID_UART Serial2
#define WIFI_PROBE Serial3

#ifdef DE_BUG    //   在COMMON.h 中启用
  #define mySerial Serial  
#endif

#define DEV_ID "T1868881605"  //设备ID号

#define TEMP_RANG_MIN  "00000000"   // 临时卡范围最小值 0
#define TEMP_RANG_MAX  "00004E20"   // 临时卡范围最大值 20000

//* SD card attached to SPI bus as follows:    
//** MOSI - pin 51
//** MISO - pin 50
//** CLK - pin 52
//** CS -  pin 53
const int led = 2;        //白色led引脚定义D2
const int sw = 3;         //门禁关闭检测引脚D3
const int led_red = 6;    //红色led未上线亮、上线熄灭、刷卡时灭一下 D6
const int led_blue = 7;   //蓝色led未上线熄灭、上线亮、刷卡时灭一下 D7
const int beep = 49;      //蜂鸣器引脚 D49  高电平响
const int locked = 42;    //继电器引脚定义 D42
const int gsm_reset = 38;  //GSM模块复位引脚定义 D38
const int rfid_reset = 4; //RFID刷卡模块复位引脚定义 D4

const char filetype[2][16]={"LIST.TXT","RECORD.TXT"};
//获取tcp232配置信息
const unsigned char get_device_info_cmd[]={0xEA,0x9B,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,
                                 0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0xB1,0xBD};
//设置tcp232配置信息
const unsigned char net_set_cmd[]={0xEA,0X9B,  //头
                       0x00,0xA6,0x65,0x34,0x2E,0x25, //MAC地址
                       0xC0,0xA8,0x00,0x08,           //模块IP
                       0x27,0x16,                     //模块PORT
                       0x6A,0x0E,0x1A,0x82,           //目标IP    //119.23.77.178  ---> 77 17 4D B2   106.14.26.130  ----> 6A 0E 1A 82 
                       0x07,0xCB,                     //目标PORT  //1995
                       0xC0,0xA8,0x00,0x01,           //网关IP
                       0xFF,0xFF,0xFF,0x00,           //子网掩码
                       0x01,                          //工作模式 0 UDP 1 TCP
                       0x00,0x25,0x80,                //串口波特率    //9600   --->   00 25 80
                       0x00,                          //数据位
                       0x00,                          //校验位
                       0x00,                          //停止位
                       0x0A,                          //串口延时时间
                       0x46,0x50,0x04,                //模块独立ID
                       0x98,0x07,                     //功能选择
                       0x22,                          //版本信息
                       0xB0};//尾
const unsigned char net_set_ret[]={0xEA,0x9B,0x00,0x00,0x45};
extern unsigned char beep_flag; //蜂鸣器标志  0 不响  1 响
extern unsigned char locked_flag;
extern unsigned char tmp_card_Enable;//临时卡开门使能标志
extern unsigned int locked_keep;//开锁时长
extern unsigned int warning_keep;//监测报警时长
extern unsigned long locked_start;//开锁起始时间
extern unsigned long calibration_seconds;  //上线时间
extern unsigned long system_seconds;        //系统时间
extern unsigned long switch_detect_start;//门禁状态监测持续开门起始时间
extern File record_File;

#endif
