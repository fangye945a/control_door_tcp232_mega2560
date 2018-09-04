#include "COMMON.h"
#include "GSM_MQTT.h"
#include "MY_RFID.h"
#include "MY_SD.h"

#define RFID_DETECT_CYCLE  300//读卡模块检测卡片周期 300ms
#define SAME_DETECT_CYCLE  2000//相同卡片读卡周期   2s
char RFID_buff[MAX_LEN_RFID]={0};
//const unsigned char rfid_init[] = {0x20,0x20,0x20,0x20,0x20,0xC1,0x41,0x00,0x79,0x03};//rfid波特率检测初始化命令
const unsigned char rfid_init[] = {0x20};
const unsigned char mifare_init[] =  {0x07,0x31,0x44,0x01,0x00,0x8C,0x03};            //mifare初始化命令
const unsigned char mifare_detect[] ={0x08,0x42,0x4D,0x02,0x00,0x26,0xDC,0x03};       //发送mifare检测命令
const unsigned char typeb_init[] =   {0x07,0x01,0x44,0x01,0x04,0xB8,0x03};            //typeb初始化命令
const unsigned char typeb_detect[] = {0x07,0x06,0x54,0x01,0x08,0xA3,0x03};            //typeb检测命令
unsigned char last_detect_type = 0;     //上次读卡检测类型  //0 S50卡   //1 身份证
unsigned long last_detect_time = 0;     //上次读卡检测时间
unsigned char detect_card_id[32] = {0}; //上次检测到卡片的ID
unsigned long last_card_time = 0;       //上次检测到卡片时间

void card_detect()  //卡片检测
{
    if( (millis() - last_detect_time) > RFID_DETECT_CYCLE)
    {
        if(last_detect_type)  //上一次读卡的类型
        {
            RFID_UART.write(mifare_init,sizeof(mifare_init));
            delay(30);
            RFID_UART.write(mifare_detect,sizeof(mifare_detect));
            last_detect_type = 0;
            last_detect_time = millis();
        }
        else
        {
            RFID_UART.write(typeb_init,sizeof(typeb_init));
            delay(30);
            RFID_UART.write(typeb_detect,sizeof(typeb_detect));
            last_detect_type = 1;
            last_detect_time = millis();
        }
    }
}


int get_Card_Id(char *str,int len,char *id)
{
  if(len == 0x0E)
  {
      unsigned char id_table[8]={0};  // 4 8
      unsigned char i=0;
      if(str[1] == 0x06)
      {
        strncpy(id_table,&str[4],8);
        sprintf(id,"%02X%02X%02X%02X%02X%02X%02X%02X",id_table[0],id_table[1],id_table[2],id_table[3],
                                                      id_table[4],id_table[5],id_table[6],id_table[7]);
      }
      else
      {
        strncpy(id_table,&str[8],4);
        sprintf(id,"%02X%02X%02X%02X",id_table[3],id_table[2],id_table[1],id_table[0]);
      }

#ifdef DE_BUG
      for (int i=0;i<len;i++) 
      {
        Serial.print("0x");
        Serial.print((unsigned char)str[i],HEX);
        Serial.print(" ");
      }
       Serial.println("");
#endif
      if( !strcmp(detect_card_id, id))  //如果与上次检测结果相同 则认为是没有拿开
      {
          if((millis() - last_card_time) > SAME_DETECT_CYCLE)  //如果与上一次检测时间差大于相同卡片读卡周期
          {
                strcpy(detect_card_id,id);  //记录此次读卡的卡ID
                last_card_time = millis();//记录此次读卡的时间
 
                return  1;
          }
          else
          {    
                 return 0; 
          }
      }
      else  //与上次结果不同
      {
          strcpy(detect_card_id,id);  //记录此次读卡的卡ID
          last_card_time = millis();//记录此次读卡的时间
          return 1;
      }
  }
  else
  {
#ifdef DE_BUG
      mySerial.println("---- Get card id error!");
#endif    
      return 0;
  }
}

void Card_task(int len)
{
    char id[20]={0};//卡片ID号字符串十六进制形式
    int ret = get_Card_Id(RFID_buff, len, id);  //获取ID
//    mySerial.print("---- The ret =");
//    mySerial.println(ret);
    if(ret > 0) //获取ID成功
    {     
          digitalWrite(beep,HIGH);  //蜂鸣器响
          digitalWrite(led_red,HIGH);  //红色LED亮一下
          digitalWrite(led_blue,LOW);  //蓝色LED灭一下
          delay(100);
          digitalWrite(beep,LOW);  //蜂鸣器不响
          if(MQTT.MQTT_Flag) //若果在线
          {
              digitalWrite(led_red,LOW);  //红色LED灭
              digitalWrite(led_blue,HIGH);  //蓝色LED亮
          }else
          {
              digitalWrite(led_red,HIGH);  //红色LED亮
              digitalWrite(led_blue,LOW);  //蓝色LED灭
          }
          
          
          ret = id_is_Authorized(id); //判断是否具有开门权限  0无权限  1有权限
          if(MQTT.MQTT_Flag == false)  //网络断开
          {
#ifdef DE_BUG
              mySerial.println("--- Device is not online!");
#endif 
              card_record_add(id, ret);  // 缓存刷卡记录
          }
          else            //网络连接正常
          {
              memset( tmp,0,sizeof(tmp) );
              sprintf(tmp,"{\"devId\":\"%s\",\"card\":\"%s\",\"state\":%d}",DEV_ID, id, ret);
 #ifdef DE_BUG
              mySerial.println(tmp);
#endif             
              MQTT.publish(0, 1, 0, MQTT._generateMessageID(), "zx/door/openlog",tmp); //上报刷卡信息
          }

          if(ret)//具有开门权限
          {
              if(locked_flag)   //是否已经开锁
              {
#ifdef DE_BUG
                  mySerial.println("*********** The door is open!!");
#endif
                  locked_start = millis();    //刷新开始时间，重新计时
              }
              else
              {
#ifdef DE_BUG
                  mySerial.println("***********  open door!!");
#endif    
                  digitalWrite(locked,HIGH);  
                  locked_start = millis();//开始计时
                  locked_flag = 1; //开锁标志置1
              }
          }
    }
}

void Serial2Event()
{
    card_detect(); //卡片检测
    if(RFID_UART.available() > 0)
    {
        int index = 0;
        memset(RFID_buff,0,sizeof(RFID_buff));
        while(RFID_UART.available() > 0)
        {
             RFID_buff[index] = RFID_UART.read();
             if(RFID_buff[index] == 0x03 && index == 0x0D)
             {          
                  Card_task(index+1);
                  while(RFID_UART.read()>=0); //清空串口缓存
                 
                  break;
             }
             if(RFID_buff[0] == 0x0E )
             {
                index++;
                delay(2);
             }   
             delay(1);
        }
    }
    
}
    
int RFID_init()
{
   last_detect_time == millis(); //初始化检测上一次卡片检测时间
   last_detect_type = 1;  //上次检测卡片的类型
   RFID_UART.write(rfid_init,sizeof(rfid_init));
   delay(100);
   RFID_UART.write(rfid_init,sizeof(rfid_init));
   delay(100);
   while(RFID_UART.available() > 0)
   {
      
     if( RFID_UART.read()  == 0x06 )
     {
#ifdef DE_BUG
          mySerial.println("RFID First init success!!");
#endif
          delay(100);
          while(RFID_UART.read()>=0); //清空串口缓存
          return 0;
     }
  }
  #ifdef DE_BUG
          mySerial.println("RFID_init success!!");
  #endif
  return 1;
}

