#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "rc522.h"
#include "usart2.h"
#include "LB301.h"
#include "dht11.h"
#include "led.h"
#include "oled.h"

/*
版本:1.0.0:
   1.简单实现RFID-RC522的ID读取并打印到串口；
	 
使用教程:
   1.将RFID-RC522与STM32F103RB开发板按下面连线方式连线；
	 2.然后将开发板通过USB转TLL连接到PC串口工具；
	 3.然后重启开发板；
	 4.将测试卡片放到感应区；
	 5.即可在串口工具看到输出卡片相关信息。
	 
RC522相关配置文件：rc522_config.h
*/

/**
*   连线说明：
*   1--SDA  <----->PA4
*   2--SCK  <----->PA5
*   3--MOSI <----->PA7
*   4--MISO <----->PA6
*   5--悬空
*   6--GND <----->GND
*   7--RST <----->PB0
*   8--VCC <----->VCC

指纹
*1 黑色-GND
*2 蓝色-PA2
*3 黄色-PA3
*4 红色 橙色-3v 


oled
   VCC----VCC(3.3)
	 GND----GND
	 SCL----B14
	 SDA----B15
	 
**/

#define usart2_baund  57600//串口二的波特率，根据指纹模块的波特率随时更改
int main(void)
{
	  u8 Temperature1;  	//温度    
    u8 Humidity1;   //湿度
    delay_init();	    	 //延时函数初始化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
    uart_init(9600);	 	//串口初始化为115200
	  LED_Init();
    RC522_Init();       //初始化射频卡模块
	  DHT11_Init();
	  OLED_Init();   //OLED初始化
	  OLED_Clear();  //OLED屏幕清屏
	  usart2_init(usart2_baund);//初始化串口2,用于与指纹模块通讯
	   while(PS_GetImage()==0xff)    //与LB301模块握手（读取图像有数据返回说明模块已经连接）
   	{
	//	printf("模块正在连接");
   	}	  
	
    	OLED_ShowCHinese(0,0,0); 
	    OLED_ShowCHinese(18,0,1);
		  OLED_ShowCHinese(36,0,2); 
	    OLED_ShowCHinese(55,0,3);
			OLED_ShowCHinese(71,0,4); 
    	OLED_ShowCHinese(88,0,5);
		  OLED_ShowCHinese(106,0,6); 
    	OLED_ShowCHinese(0,2,7);
		// 	GPIO_SetBits(GPIOA,GPIO_Pin_1);						 //PB.5 输出高	 
		 // GPIO_SetBits(GPIOB,GPIO_Pin_12);						 //PB.5 输出高
    while(1)
    {  
			 	GPIO_SetBits(GPIOB,GPIO_Pin_1);						 //PB.5 输出高	
		  DHT11_Read_Data(&Temperature1,&Humidity1);	//读取温湿度值
    	printf("n5.val=%d",Temperature1);
			printf("%c",0xff);
    	printf("%c",0xff);
	    printf("%c",0xff);
			printf("n6.val=%d",Humidity1);
			printf("%c",0xff);
    	printf("%c",0xff);
	    printf("%c",0xff);
				OLED_ShowCHinese(0,4,40); 
	OLED_ShowCHinese(18,4,41);
	OLED_ShowNum(36,4,Temperature1,2,16);
	
		OLED_ShowCHinese(72,4,42); 
	OLED_ShowCHinese(90,4,43);
	OLED_ShowNum(106,4,Humidity1,2,16);
			
			
			if(People==0)
			{     
				    GPIO_SetBits(GPIOB,GPIO_Pin_9);						 //PB.5 输出高
					  delay_ms(1000);
						GPIO_ResetBits(GPIOB,GPIO_Pin_9);						 //PB.5 输出高
			}
			if(Fire==0)
			{   GPIO_SetBits(GPIOB,GPIO_Pin_12);						 //PB.5 输出高
			    GPIO_SetBits(GPIOB,GPIO_Pin_9);						 //PB.5 输出高
					delay_ms(1000);
					GPIO_ResetBits(GPIOB,GPIO_Pin_9);						 //PB.5 输出高
				GPIO_ResetBits(GPIOB,GPIO_Pin_12);						 //PB.5 输出高
			}		
    //RC522_Handel();
		//Add_FR();
	  press_FR();
    }
}


