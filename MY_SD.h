#ifndef _MY_SD_H_
#define _MY_SD_H_
#include <SPI.h>
#include <SD.h>
#define ID_LINE_LEN 64  //文件中ID一行的长度

extern File myFile;
int microsd_init();

int miscrosd_change_id(int type,char *cardID,unsigned char add_or_delete);

int microsd_add_id(int type, char *str_id);

int microsd_delete_id( char *str_id );

int id_is_Authorized(char *id);   //ID是否授权

int id_is_exist(char *id);

int get_id_pos(char *id);

int read_a_line(File myFile,char *str);

void card_record_add(char *str_id, int state);  //添加刷卡记录

void put_record(File myFile);  //上报一条刷卡记录

void Filedisplay(int type);

#endif 
