#include "COMMON.h"
#include "WIFI_PROBE.h"
#include "GSM_MQTT.h"
#include "MY_SD.h"
#include "MY_RFID.h"

#define LOCK_KEEP_MAX 30                //开锁最大时长 3s  单位100ms
#define RECONNECT_TIMES_THRESHOLD  10    //进入重连等待的失败次数      
#define RECONNECT_WAIT_TIME  600000         //重连等待时间 单位ms

unsigned int locked_keep = 0;//开锁时长  单位 100ms
unsigned int warning_keep = 0; //持续开门报警时长 单位分钟
unsigned long locked_start = 0;//控制开锁起始时间
unsigned long switch_detect_start = 0;//门禁状态监测持续开门起始时间
unsigned long calibration_seconds = 0;  //上线校准时间
unsigned long system_seconds = 0;        //上线时系统时间
unsigned long current_seconds = 0;        //当前时系统时间
unsigned char try_connect_times = 0; 		//MQTT连接尝试次数
unsigned char beep_flag = 0; //蜂鸣器标志  0 不响  1 响
unsigned char tmp_card_Enable = 0;//临时卡开门使能标志
unsigned char locked_flag = 0;//开锁标志  0 关   1 开

unsigned char device_info_buff[44]={0}; //保存TCP232配置信息 
unsigned char device_info_index = 0; //保存TCP232配置信息
unsigned char device_info_flag = 0;
unsigned long PrevMillis_connect = 0;
unsigned long CurMillis_connect = 0;

#ifdef DE_BUG
	unsigned char loop_enter_flag = 0;
#endif

void Param_init()
{
    int eeAddress = 0;              //开门时长
    EEPROM.get(eeAddress,locked_keep);  //获取EEPROM中开门时长   默认为10  单位100ms
    if(locked_keep == 0x0 || locked_keep == 0xFFFF || locked_keep > LOCK_KEEP_MAX )  //若为初始值
    {
        locked_keep = 10;
        EEPROM.put(eeAddress,locked_keep);
    }
    eeAddress = sizeof(locked_keep);     //临时卡开门使能标志  默认为0  不开启
    EEPROM.get(eeAddress,tmp_card_Enable);//获取EEPROM中临时卡开门使能标志  默认为0  不开启
    if(tmp_card_Enable == 0xFF) //若为初始值
    {
        tmp_card_Enable = 0;
        EEPROM.put(eeAddress,locked_keep);
    }
    
    eeAddress += sizeof(tmp_card_Enable);   //持续开门报警时长
    EEPROM.get(eeAddress,warning_keep);//获取EEPROM中持续开门报警时长  默认为15  单位min
    if(warning_keep == 0x0 || warning_keep == 0xFFFF) //若为初始值或不在规定范围内
    {
        warning_keep = 15;
        EEPROM.put(eeAddress,warning_keep);
    }
    
   // eeAddress += sizeof(warning_keep);   //MQTT重连尝试失败次数
   // EEPROM.get(eeAddress,restart_times);//获取EEPROM中重连尝试次数
    //if(restart_times == 0xFF) //若为初始值
    //{
    //   restart_times = 0;
    //  EEPROM.put(eeAddress,restart_times);
    //}
    
    eeAddress += sizeof(unsigned char);   //系统起始时间
    EEPROM.get(eeAddress,calibration_seconds);//获取EEPROM中重连尝试次数
    if(calibration_seconds == 0xFFFFFFFF) //若为初始值
    {
        calibration_seconds = 0;
        EEPROM.put(eeAddress,calibration_seconds);
    }
     
#ifdef DE_BUG
    mySerial.print("EEPROM.length = ");
    mySerial.println(EEPROM.length());
    mySerial.print("locked_keep =");
    mySerial.println(locked_keep);
    mySerial.print("tmp_card_Enable =");
    mySerial.println(tmp_card_Enable);
    mySerial.print("warning_keep =");
    mySerial.println(warning_keep);
    mySerial.print("calibration_seconds =");
    mySerial.println(calibration_seconds);
    mySerial.print("SERIAL_RX_BUFFER_SIZE = ");
    mySerial.println(SERIAL_RX_BUFFER_SIZE,DEC);
#endif
}


void Parse_Json_message_Task(char *json)
{
    StaticJsonBuffer<MESSAGE_BUFFER_LENGTH> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    if (!root.success()) 
    {
#ifdef DE_BUG
        mySerial.println("parseObject() failed"); //解析对象失败
#endif
        return;
    }
    int optcode = root["optcode"];
#ifdef DE_BUG
        mySerial.print("optcode="); //对应操作命令
        mySerial.println(optcode);
#endif
    switch(optcode)
    {
        case 1:                         //开锁
        {
            if(locked_flag)   //是否已经开锁
                locked_start = millis();    //刷新开始时间，重新计时
            else
            {
#ifdef DE_BUG
                mySerial.println("--------- open door!!");
#endif
                digitalWrite(locked,HIGH);  
                locked_start = millis();//开始计时
                locked_flag = 1; //开锁标志置1
            }
        }
        break;
        case 2:                         //推送黑白名单
        {
            int type = root["type"];
            unsigned char add_or_delete = root["add"];
            const char *p = root["card"];
            char cardID[32]={0};
            memset(cardID,0,sizeof(cardID));
            strcpy(cardID,p);
            miscrosd_change_id(type,cardID,add_or_delete);//增删名单
        }
        break;
        case 3:                         //临时卡开门功能开启或关闭
        {
            unsigned char type = root["type"];
            int eeAddress = sizeof(unsigned int);
            if(tmp_card_Enable != type)
            {
                tmp_card_Enable = type;
                EEPROM.put(eeAddress,tmp_card_Enable);//写入EEPROM中临时卡开门使能标志
            }
        }
        break;
        case 4:                         //设置开门时长
        {
            int eeAddress = 0;
            unsigned int keeptime = root["time"];
            if(locked_keep != keeptime)
            {
                locked_keep = keeptime;
                EEPROM.put(eeAddress,locked_keep);//写入EEPROM中临时卡开门使能标志
            }
        }
        break;
        case 7:                         //校准时间
        {
            calibration_seconds = root["time"];  //当前网络时间
            system_seconds = millis()/1000;       //系统当前时间
#ifdef DE_BUG
            mySerial.print("now_time = ");
            mySerial.println(calibration_seconds);
#endif
            int eeAddress = sizeof(warning_keep)+sizeof(tmp_card_Enable)+sizeof(locked_keep)+sizeof(unsigned char);
            EEPROM.put(eeAddress,calibration_seconds); //存入EEPROM
        }
        break;
        case 8:                                   //持续开门报警时长设置
        {
            int eeAddress = sizeof(unsigned int) + sizeof(unsigned char);
            unsigned int optim = root["optim"];  //持续开门报警时长
            if( optim != warning_keep )
            {
                warning_keep = optim;
                EEPROM.put(eeAddress,warning_keep);//写入EEPROM中持续开门报警时长
            }
        }
        break;
        case 9:                         //黑白名单批量导入删除
        {
            int ret = 0;
            unsigned int len = root["length"];
            int type = root["type"];     
            char cardID[32]={0};
            unsigned char add_or_delete = root["add"];
            char *p = NULL;
#ifdef DE_BUG
            mySerial.print("length = ");
            mySerial.println(len);
#endif
            for(int i=0; i<len; i++)
            {
                p  = root["cards"][i];
                if(p == NULL)
                {
                    ret -= 1;
                    continue;
                }memset(cardID,0,sizeof(cardID));
                strcpy(cardID,p);
                ret += miscrosd_change_id(type,cardID,add_or_delete);
            }
            memset( tmp,0,sizeof(tmp) );
            if(ret < 0 || len == 0)
              sprintf(tmp,"{\"devId\":\"%s\",\"optcode\":9,\"type\":%d,\"add\":%d,\"state\":0}",DEV_ID,type,add_or_delete);
            else
              sprintf(tmp,"{\"devId\":\"%s\",\"optcode\":9,\"type\":%d,\"add\":%d,\"state\":1}",DEV_ID,type,add_or_delete);
#ifdef DE_BUG
            mySerial.println(tmp);
#endif     
          MQTT.publish(0, 1, 0, MQTT._generateMessageID(), "zx/door/optack",tmp);
        }
        break;
        case 110:                       //查询设备状态,推送上线消息
        {
            memset( tmp,0,sizeof(tmp) );
            sprintf(tmp,"{\"devId\":\"%s\",\"state\":1,\"signal\":%s}",DEV_ID,csq);
            MQTT.publish(0, 1, 0, MQTT._generateMessageID(), "zx/door/firstword",tmp);
        }
        break;
        case 120:                       //重启设备
        {
#ifdef DE_BUG
            mySerial.println("rebooting...");
#endif
            MQTT.disconnect();  //断开连接
            asm volatile ("jmp 0");
        }
        break;
        default:
#ifdef DE_BUG
        mySerial.println("error optcode!!");
#endif
        break;
    }
}

void IO_init()
{
    unsigned char i = 0;
    pinMode(53, OUTPUT);           //SD卡
    pinMode(led, OUTPUT);          //设置IO口为输出模式  白色led
    pinMode(sw, INPUT);            //设置IO口为输入模式  门禁检测
    pinMode(led_red, OUTPUT);      //设置IO口为输出模式  红色led
    pinMode(led_blue, OUTPUT);     //设置IO口为输出模式  蓝色led
    pinMode(locked ,OUTPUT);       //继电器控制引脚
    pinMode(gsm_reset ,OUTPUT);   //GSM模块复位引脚
    pinMode(rfid_reset,OUTPUT); 	//RFID复位初始化
    pinMode(beep,OUTPUT);  			//Beep模块初始化
    
    digitalWrite(locked,LOW);        //继电器IO初始化为低电平
    digitalWrite(led,LOW);           //白色led灯点熄灭
    digitalWrite(led_red,HIGH);      //红色led灯点亮
    digitalWrite(led_blue,LOW);      //蓝色led灯点熄灭
	
    digitalWrite(gsm_reset,LOW);  //GSM模块复位初始化
    digitalWrite(rfid_reset,LOW);  //RFID读卡模块复位初始化
    digitalWrite(beep,HIGH);  //蜂鸣器引脚置高   高电平响  低电平不响 
    delay(500);					  //置低500ms然后拉高
    digitalWrite(rfid_reset,HIGH);
    digitalWrite(gsm_reset,HIGH); //GSM模块复位初始化
    
    digitalWrite(beep,LOW);  //蜂鸣器引脚置低   高电平响  低电平不响
    digitalWrite(led,HIGH);           //白色led灯点
}

void URAT_init()
{
#ifdef DE_BUG
    mySerial.begin(115200);
#endif
    GSM_UART.begin(9600);   //GSM串口初始化
    RFID_UART.begin(9600);  //读卡模块初始化
    WIFI_PROBE.begin(115200);//探针模块初始化
}

#ifdef DE_BUG
void SerialEvent()
{
    if(mySerial.available())
    {
        int index = 0;
        while(mySerial.available())
        {
            tmp[index++]=mySerial.read();
            delay(2);
        }
        if( !strncmp(tmp,"ls",2))
        {
     
      		mySerial.println("--------- WHITE AND BLACK");
      		index = SD.exists("LIST.TXT");
      		if(index)
      			Filedisplay(0);//查看sd卡中的文件   
      		else
      			mySerial.println("The file not exist!");
      
      
      		mySerial.println("--------- RECORD");
      		index = SD.exists("RECORD.TXT");
      		if(index)
      			Filedisplay(1);//查看sd卡中的文件   
      		else
      			mySerial.println("The file not exist!");
        }
        else if(!strncmp(tmp,"time",4))
        {
          mySerial.print("nowtime:");
          mySerial.println(millis());
        }
        else if(!strncmp(tmp,"rm",2))
        {
           if(SD.exists("LIST.TXT"))
           {
              SD.remove("LIST.TXT");
           }
           mySerial.println("remove finish!");
        }
    }
    wdt_reset(); //喂狗操作
}
#endif

void DoorEvent()
{
    current_seconds = millis();
    if(locked_flag && (current_seconds - locked_start  >=  locked_keep*100) )   //开门时长定时
    {
        locked_flag = 0;
        digitalWrite(locked,LOW);
#ifdef DE_BUG
        mySerial.println("*********** close door!!");
#endif
    }
    
    unsigned int sw_state = digitalRead(sw);
  //  delay(30);  //消抖
    if(sw_state != digitalRead(sw))
          return;
   
    if( sw_state ) //高电平门禁打开
    {
        if(!switch_detect_start)  //如果没有开始计时
        {
#ifdef DE_BUG
            mySerial.println("switch_detect_start!!");
#endif
            switch_detect_start = millis();//开始计时
        }
        else
        {
            if(!beep_flag  &&  (millis() - switch_detect_start) >= (warning_keep * 60000) )
            {
#ifdef DE_BUG
                mySerial.println("beep open!!");
#endif
                beep_flag = 1;        //蜂鸣器标志置1
                digitalWrite(beep,HIGH); //打开蜂鸣器
                
                memset( tmp,0,sizeof(tmp) );               //持续开门报警上报
                unsigned long now_time = calibration_seconds + ( (millis()/1000) - system_seconds);
                sprintf(tmp,"{\"devId\":\"%s\",\"time\":%ld,\"opentime\":%d}",DEV_ID, now_time, warning_keep);
                MQTT.publish(0, 1, 0, MQTT._generateMessageID(), "zx/door/opalarm",tmp);
            }
        }
    }
    else //低电平门禁关闭
    {
        if(switch_detect_start) //如果已经开始计时
        {
            switch_detect_start = 0;
#ifdef DE_BUG
            mySerial.println("switch_detect_count clean!!");
#endif           
            if(beep_flag)  //如果正在响
            {
#ifdef DE_BUG
                mySerial.println("*********** beep close!!");
#endif           
                beep_flag = 0; //蜂鸣器标志置0
                digitalWrite(beep,LOW);//关闭蜂鸣器
            }
        }
    }
    wdt_reset(); //喂狗操作
}

void TimeEvent()
{
    current_seconds = millis();
    if( (current_seconds/1000) - system_seconds >= 60 ) //每分钟进行校时
    {
        calibration_seconds = calibration_seconds + ( (current_seconds/1000) - system_seconds);
        system_seconds = current_seconds/1000;  //更新对应系统时间
        int eeAddress = sizeof(locked_keep) + sizeof(tmp_card_Enable) + sizeof(warning_keep) + sizeof(unsigned char);
        EEPROM.put(eeAddress,calibration_seconds);  //存入EEPROM
#ifdef DE_BUG
        mySerial.print("nowtime =");
        mySerial.println(calibration_seconds);
#endif 
    }
}

void setup()
{
    URAT_init();   //串口初始化
    IO_init();     //IO口初始化
    Param_init();   //参数初始化
    while(microsd_init());   //SD卡初始化
    RFID_init();      //RFID读卡模块初始化    
    wdt_enable(WDTO_8S); //开启看门狗，并设置溢出时间为8秒
#ifdef DE_BUG
    mySerial.println("setup finish!!");
	mySerial.println("Wait around 10s for TCP232 connect to mqttserver!!");
#endif 
}

void loop()
{	
    MQTT.processing(); // 联网处理任务
    Serial1Event();    //MQTT任务    
    Serial2Event();    // 读卡任务
    SerialEvent();     // 调试串口任务
	WifiProbeEvent();  // wifi探针任务
	DoorEvent();       // 开门检测任务
	TimeEvent();       // 时钟校准任务
	wdt_reset(); 		//喂狗操作
}
