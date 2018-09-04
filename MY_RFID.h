#ifndef _MY_RFID_H_
#define _MY_RFID_H_

#define MAX_LEN_RFID  256

void Serial2Event();
void card_detect();  //卡片检测
int get_Card_Id(char *str,int len,char *id);
int RFID_init();

#endif 
