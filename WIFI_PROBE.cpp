#include "WIFI_PROBE.h"
#include "COMMON.h"
#include "GSM_MQTT.h"

WIFI_PROBE_INFO wifi_probe_devices[WIFI_PROBE_MAX_DEVICE]={0};
char wifi_probe_buff[WIFI_PROBE_MAX_BUFF] = {0};
const int signal_threshold = -80;//信号强度阈值

void upload_device_info(char *device_mac)
{
    memset(tmp,0,sizeof(tmp));
    sprintf(tmp,"{\"devId\":\"%s\",\"macs\":[\"%s\"],\"length\":1}",DEV_ID, device_mac);
#ifdef DE_BUG
    mySerial.println(tmp);
#endif             
    MQTT.publish(0, 1, 0, MQTT._generateMessageID(), "zx/door/macs",tmp); //上报MAC信息
    return;
}

void record_device(char *mac) //更新和记录设备信息
{
    int i = 0;
    int min_index = -1;  //没有记录数据的最小下标
  	int min_time_index = -1;  //信号最弱的记录下标
  	unsigned long time_tmp = millis();
    for(i=0; i<WIFI_PROBE_MAX_DEVICE; i++)  //遍历设备信息缓存数组
    {
        if( wifi_probe_devices[i].enable_flag == 1 ) //是否正在记录数据
        {
            if(!strcmp( wifi_probe_devices[i].device_mac, mac)) //如果找到对应mac
            {
                if(millis() - wifi_probe_devices[i].probe_time > WIFI_PROBE_UPLOAD_CYCLE)  //时间间隔大于3min  则上传记录 更新时间
                {
                    wifi_probe_devices[i].probe_time = millis();  //更新时间
                    upload_device_info(mac);//上传设备信息
                }
                return;
            }
            
      			if(wifi_probe_devices[i].probe_time < time_tmp)  //找到时间最远的数据下标
      			{
        				time_tmp = wifi_probe_devices[i].probe_time;
        				min_time_index = i;
      			}	
        }
        else
        {
            if(min_index == -1)
                min_index = i;
        }
    }
    if(min_index == -1)   //如果设备数据缓存队列已满 覆盖时间最远的设备记录
    {
        wifi_probe_devices[min_time_index].enable_flag = 1;
        strcpy(wifi_probe_devices[min_time_index].device_mac, mac);//保存MAC地址
        wifi_probe_devices[min_time_index].probe_time = millis(); //更新时间
        upload_device_info(mac);
    }
    else  //添加设备信息至队列并上传记录
    {
    		wifi_probe_devices[min_index].enable_flag = 1;
    		strncpy(wifi_probe_devices[min_index].device_mac, mac, 17);//保存MAC地址
        wifi_probe_devices[min_index].probe_time = millis(); //更新时间
        upload_device_info(mac);
    }
}

void get_device_info(char *buff)
{
    char mac[18]={0};
    int signal_probe = 0;
    int ret = sscanf(buff,"%[^|]|%*[^|]|%*[^|]|%*[^|]|%*[^|]|%d",mac,&signal_probe);
    if(ret == 2)
    {
        if(signal_probe >= signal_threshold)
        {
           record_device(mac); //更新和记录设备信息   
        }
    }
    return;
}

void WifiProbeInit()
{
    WIFI_PROBE.write("AT+CH0\r\n");
    Serial.println("WifiProbeInit...");
}

void WifiProbeEvent()  // wifi探针任务
{
  char ch = 0;
  if(MQTT.MQTT_Flag == false)    //没有联网则不开始此任务
  {
      while(ch >= 0) //清空单条串口缓存
      {
          ch = WIFI_PROBE.read();
          if(ch == '\n')
            break;
      }
      return;
  }
	if(WIFI_PROBE.available() >= 0) //如果缓冲区有数据
	{
    int index = 0;
		memset(wifi_probe_buff,0,sizeof(wifi_probe_buff)); //清空缓冲区
		while(WIFI_PROBE.available())
		{
      if(WIFI_PROBE.available()<5)
        delay(2);
			ch = WIFI_PROBE.read();
			if(ch == '\r')
                continue;
       if(ch == '\n')
                break;
			wifi_probe_buff[index++] = ch;
      if(index >= WIFI_PROBE_MAX_BUFF-2)  //防止越界
        break;
		}
   
		if(index == 48)
		{
//#ifdef DE_BUG
//        mySerial.println(wifi_probe_buff);
//#endif
        	get_device_info(wifi_probe_buff);	//获取设备ID
		}
    else if(index > 48)
    {
  		if(ch != '\n' && WIFI_PROBE.available()>0)
    	{		
          while(ch >= 0) //清空单条串口缓存
          {
              ch = WIFI_PROBE.read();
              if(ch == '\n')
                break;
          }
    	}
    }
	}
  return;   
}
