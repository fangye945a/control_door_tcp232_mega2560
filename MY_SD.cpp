#include "COMMON.h"
#include "MY_SD.h"
#include "GSM_MQTT.h"

File myFile;
File record_File;

unsigned long starttime=0;
unsigned long endtime=0;
char message_tmp[256]={0};
void(* resetFunc) (void) = 0;

int microsd_init()
{
    if (!SD.begin(53)) 
    {
#ifdef DE_BUG
        mySerial.println("SD detect failed!");
#endif
        return 1;
    }
    else 
    {
#ifdef DE_BUG
        mySerial.println("SD detect success!");
#endif
        return 0;
    } 
}

int id_is_Authorized(char *id)   //ID是否授权
{
    int ret = id_is_exist(id);   //查询是否为白名单
    if(ret == 1)       //白名单
        return 1;
    else if(ret == 2)  //黑名单
        return 0;    
    if( tmp_card_Enable
        && (strcmp(id,TEMP_RANG_MIN) >= 0)
        && (strcmp(id,TEMP_RANG_MAX) <= 0)
        )   //查询是否为临时名单 且 临时卡开门标志是否使能
        return 1;
    return 0;
}
int get_id_pos( char *id )
{
    int ret = 0;
    char str[ID_LINE_LEN]={0};
    ret = SD.exists(filetype[0]);
    if(ret == 0)
    {
        //mySerial.println("The file not exist!");
        return -1;
    }
    
    myFile = SD.open(filetype[0],FILE_READ);
    if (myFile)
    {    
        while(myFile.available())
        {
            ret = read_a_line(myFile,str);
            if( !strncmp(id,str,strlen(id)) && str[strlen(id)] == ' ')
            {
                myFile.close();
                return ret;
            }
        }
        myFile.close();
        return -1;
    }
    else
      mySerial.println("open file failed!");
    return 0;
}
int id_is_exist(char *id)
{
    int ret = 0;
    char str[ID_LINE_LEN]={0};
    ret = SD.exists(filetype[0]);
    if(ret == 0)
    {
       // mySerial.println("The file not exist!");
        return 0;
    }
    myFile = SD.open(filetype[0],FILE_READ);
    if (myFile)
    {    
        while(myFile.available())
        {
            ret = read_a_line(myFile,str);
            if( !strncmp(id,str,strlen(id)) && str[strlen(id)] == ' ')
            {
                myFile.close();
                if(str[strlen(str)-1] == '1')
                  return 1;
                else if(str[strlen(str)-1] == '2')
                  return 2;
                else
                  return 0;
            }
        }
        myFile.close();
        return 0;
    }
    else
      mySerial.println("open file failed!");
    return 0;
}


int read_a_line(File myFile,char *str)
{
    if(str == NULL)
      return -1;
    int i=0;
    while(myFile.available())
    {
         str[i++] = myFile.read();
         if(str[i-2] == '\r' && str[i-1] == '\n' )
         {
            str[i-2] = '\0';
            return myFile.position()-i;
         }
    }
    str[i] = '\0';
    return myFile.position()-i;
}
int miscrosd_change_id(int type,char *cardID,unsigned char add_or_delete)
{
    if(add_or_delete)  //添加名单
    {
        return microsd_add_id(type,cardID);
    }
    else                //删除名单
    {
        return microsd_delete_id(cardID);
    }
}
int microsd_add_id(int type,char *str_id)
{
    char str[ID_LINE_LEN]={0};
    if(type !=0 && type != 1)
    {
        mySerial.println("microsd_add_id type error!!");
        return -1;
    }
    if( !SD.exists(filetype[0]))
    {
        mySerial.print("Create ");
        mySerial.println(filetype[0]);
    }
    int pos = get_id_pos(str_id);
    if(pos == -1)
    {
        myFile = SD.open(filetype[0], FILE_WRITE);
        delay(2);
        if (myFile)
        {
            
            if(type == 1)
               strcat(str_id," 1");
            else if(type == 0)
               strcat(str_id," 2");
            myFile.println(str_id);
            mySerial.print("Add id to ");
            mySerial.println(filetype[0]);
            myFile.close();
        }
        else
        {
          mySerial.println("open file failed!");
          return -1;
        }
        return 0;
    }
    else
    {
        myFile = SD.open(filetype[0],O_RDWR | O_CREAT);
        if (myFile)
        {
            
            myFile.seek(pos);
            read_a_line(myFile,str);
            int len = strlen(str);
            mySerial.print("str[len-1] = ");
            mySerial.println(str[len-1]);
            if( (type == 0 && str[len-1] != '2')
              ||(type == 1 && str[len-1] != '1'))
            {
              
              mySerial.print("Add id to ");
              mySerial.println(filetype[0]);
              myFile.seek(pos+len-1);
              if(type == 1)
                  myFile.println('1');
              else if(type == 0)
                  myFile.println('2');
            }
            else
            {
                mySerial.print("ID has been added to ");
                mySerial.println(filetype[0]);
            }
        }else
        {
            mySerial.println("open file failed!");
            return -1;
        }
        myFile.close();
        return 0;
    }
}
int microsd_delete_id( char *str_id)
{
    char str[ID_LINE_LEN]={0};
    if( !SD.exists(filetype[0]))
    {
        mySerial.print("ID has been delete from ");
        mySerial.println(filetype[0]);
    }
    int pos = get_id_pos(str_id);
    if(pos == -1)
    {
        mySerial.print("ID has been delete from ");
        mySerial.println(filetype[0]);
        return 0;
    }
    else
    {
        myFile = SD.open(filetype[0],O_RDWR | O_CREAT);
        if (myFile)
        {
            myFile.seek(pos);
            read_a_line(myFile,str);
            int len = strlen(str);
            if( str[len-1] == '1' ||  str[len-1] == '2' )
            {
                myFile.seek(pos+len-1);
                myFile.println('0');
                mySerial.print("Delete id from ");
                mySerial.println(filetype[0]);
            }
            else
            {
                mySerial.print("ID has been delete from ");
                mySerial.println(filetype[0]);
            }
        }else
        {
             mySerial.println("open file failed!");
             return -1;
        }
        myFile.close();
        return 0;
    }
}


void card_record_add(char *str_id, int state)  //添加刷卡记录
{
    char str[ID_LINE_LEN]={0};
    myFile = SD.open(filetype[1], FILE_WRITE);
    delay(2);
    if (myFile)
    {
        unsigned long now_time = calibration_seconds + ( (millis()/1000) - system_seconds);
        sprintf(str,"%s %ld %d 1",str_id,now_time,state);
        myFile.println(str);
    }
    else
      mySerial.println("open file failed!");
    myFile.close();
    return 0;
}
int get_record_num()  //获得离线刷卡刷卡记录条数
{
    int num = 0;
    char str[ID_LINE_LEN]={0};
    myFile = SD.open(filetype[1], FILE_READ);
    delay(2);
    if (myFile)
    {
      while(myFile.available())  
      {
          read_a_line(myFile,str);
          num++;
          wdt_reset(); //喂狗操作
      }
    }
    else
      mySerial.println("open file failed!");
    myFile.close();
    return num;
}
void put_record(File myFile)  //上报一条刷卡记录
{
    if(myFile.available()>0)
    {
        char str[32]={0};
        char id[16]={0};
        char record_time[16]={0};
        unsigned int state = 0;
        unsigned int isenable = 0;
        unsigned int pos = read_a_line(myFile,str);  //读取一行数据
        int ret = sscanf(str,"%s %s %d %d",id, record_time, &state, &isenable);
        if(ret == 4)
        {
            //mySerial.println("Parsing success!");
            if(isenable)  //未上报则上报记录
            {
                memset( tmp,0,sizeof(tmp) );
                sprintf(tmp,"{\"devId\":\"%s\",\"time\":%s,\"card\":\"%s\",\"state\":%d}",DEV_ID, record_time, id, state);
#ifdef DE_BUG
                mySerial.println(tmp);
#endif             
                MQTT.publish(0, 1, 0, MQTT._generateMessageID(), "zx/door/openlog",tmp); //上报刷卡信息
                myFile.seek(pos+strlen(str)-1);
                myFile.println('0');
            }
        }
    }
    else
    {
        myFile.close();
        SD.remove(filetype[1]); //删除刷卡记录文件
    }
    return;
}

void Filedisplay(int type)
{
    if(type != 0 && type != 1 && type != 2)
    {
      mySerial.println("type error!");
      return;
    }
    char str[ID_LINE_LEN]={0};
    int lines = 0;
    int ret = 0;
    myFile = SD.open(filetype[type],FILE_READ);
    if (myFile)
    {
        mySerial.print(filetype[type]);
        mySerial.println(":");
       
        while(myFile.available())
        {
            ret = read_a_line(myFile,str);
           // mySerial.print("position = ");
           // mySerial.println(ret);
           // mySerial.print("context = ");
            mySerial.print(lines);
            mySerial.print(":");
            mySerial.println(str);
            wdt_reset(); //喂狗操作
           // mySerial.print("line = ");
            lines++;
        }
        myFile.close();
    }
    else
      mySerial.println("open file failed!");
}
