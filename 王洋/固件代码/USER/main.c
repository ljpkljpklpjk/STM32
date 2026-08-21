#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "rc522.h"
#include "oled.h"
#include "adc.h"
#include "dht11.h"
#include "led.h"

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
*   8--VCC <----->VCC(3.3)

oled
   VCC----VCC(3.3)
	 GND----GND
	 SCL----B14
	 SDA----B15
	 
mq135
   VCC----5V
	 GND----GND
	 AO-----PA0
	 
火灾
   VCC----5V
	 GND----GND
	 A0-----PA1
	 
**/
//void Test_Oled()
//{
//	OLED_ShowCHinese(0,0,0); 
//	OLED_ShowCHinese(18,0,1); 
//	OLED_ShowCHinese(0,2,2); 
//	OLED_ShowCHinese(18,2,3); 
//	OLED_ShowCHinese(0,4,8);
//    OLED_ShowCHinese(18,4,9);
//    OLED_ShowCHinese(36,4,10);
//    OLED_ShowCHinese(54,4,11);   
//}
   // int person=30;
	  u16 AirQuality1;   //空气质量
	  u16 Fire1;   //火灾情况
		u8 Temperature1;  	//温度    
    u8 Humidity1;   //湿度
    // u16 In;   //
	//  u16 Out;   //
void send1()
{
	printf("%c",0xff);
	printf("%c",0xff);
	printf("%c",0xff);
}
	void Control1()
{
	DHT11_Read_Data(&Temperature1,&Humidity1);	//读取温湿度值
	OLED_ShowCHinese(0,0,0); 
	OLED_ShowCHinese(18,0,1);
  OLED_ShowNum(36,0,Temperature1,2,16);
	OLED_ShowCHinese(55,0,2); 
	OLED_ShowCHinese(71,0,1);
	OLED_ShowNum(87,0,Humidity1,2,16);
  printf("page1.n0.val=%d",Temperature1);  
	send();
	printf("page1.n1.val=%d",Humidity1); 
	send1();
  AirQuality1=Get_Adc(ADC_Channel_0);
	AirQuality1=(3900-AirQuality1)/39;
		printf("page1.n2.val=%d",AirQuality1);  
	send1();
  //printf("%d\r\n",AirQuality);
	OLED_ShowCHinese(0,2,9); 
	OLED_ShowCHinese(18,2,10);
  OLED_ShowNum(36,2,AirQuality1,2,16);
	
	OLED_ShowCHinese(0,4,26); 
	OLED_ShowCHinese(18,4,27);
	OLED_ShowCHinese(36,4,28); 
	OLED_ShowCHinese(54,4,29);
	Fire1=Get_Adc(ADC_Channel_1);
	if(Fire1<500)
	{
		OLED_Clear();  //OLED屏幕清屏
		OLED_ShowCHinese(0,0,13); 
	  OLED_ShowCHinese(18,0,14);
		OLED_ShowCHinese(32,0,15);
		OLED_ShowCHinese(48,0,16);
		GPIO_SetBits(GPIOB,GPIO_Pin_7);					 //小太阳
					   GPIO_SetBits(GPIOB,GPIO_Pin_8);						 //待定
		printf("\xff\xff\xff");      //操作串口屏跳转界面
    printf("page 2\xff\xff\xff");      //操作串口屏跳转界面
		delay_ms(1000);
		OLED_Clear();  //OLED屏幕清屏
		GPIO_ResetBits(GPIOB,GPIO_Pin_7);					 //小太阳
		GPIO_ResetBits(GPIOB,GPIO_Pin_8);						 //待定
		printf("\xff\xff\xff");      //操作串口屏跳转界面
    printf("page 0\xff\xff\xff");      //操作串口屏跳转界面
	}
	  if(In==0)
							 {
								 	printf("\xff\xff\xff");      //操作串口屏跳转界面
                  printf("page 3\xff\xff\xff");      //操作串口屏跳转界面
								   GPIO_SetBits(GPIOB,GPIO_Pin_8);						 //待定
								
								 	delay_ms(1000);
						      delay_ms(1000);
								 	printf("\xff\xff\xff");      //操作串口屏跳转界面
                  printf("page 0\xff\xff\xff");      //操作串口屏跳转界面
								  GPIO_ResetBits(GPIOB,GPIO_Pin_8);						 //待定
							
							 }
}

int main(void)
{  
	int z=10;
//	  int person=30;
//	  u16 AirQuality;   //空气质量
//	  u16 Fire;   //火灾情况
//	  u16 People;
    delay_init();	    	 //延时函数初始化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
    uart_init(9600);	 	//串口初始化为115200
    RC522_Init();       //初始化射频卡模块
	  DHT11_Init();  //dht11初始化
	  OLED_Init();   //OLED初始化
	  OLED_Clear();  //OLED屏幕清屏
	  Adc_Init();   //ADC初始化 读取空气质量 火灾
  	LED_Init(); //自动控制初始化
	//  printf("ok");
    while(1)
    {   
			GPIO_ResetBits(GPIOC,GPIO_Pin_13);						 //待定
			//DHT11_Read_Data(&Temperature,&Humidity);	//读取温湿度值
		//printf("%d\r\n",Temperature);
//		  AirQuality=Get_Adc(ADC_Channel_0);
//			AirQuality=(3900-AirQuality)/39;
//  		printf("%d\r\n",AirQuality);
				//People=Get_Adc_Average(ADC_Channel_2,10);
			//printf("%d\r\n",People);
			//delay_ms(100);
//				Fire=Get_Adc(ADC_Channel_1);
//		  	printf("%d\r\n",Fire);
//			//Text_Temperature();
			//Test_Oled();  //测试oled
   RC522_Handel();   //RFID程序读取
	  Control1();
//	if(USART_ReceiveData(USART1)==03)
//	{
//	 GPIO_SetBits(GPIOC,GPIO_Pin_13);						 //待定
//		z=z-1;
//		printf("n7.val=%d",z); 
//	  send1();
//		
//	}
//     In=Get_Adc(ADC_Channel_3);
//		 printf("In=%d\r\n",In);
//		Out=Get_Adc(ADC_Channel_2);
//				 printf("out=%d\r\n",Out);
//			delay_ms(1000);
//			delay_ms(1000);
////			
//			if(Peo==1)
//			{
//			GPIO_SetBits(GPIOB,GPIO_Pin_7);						 //待定
//				delay_ms(1000);
//			}
  // GPIO_SetBits(GPIOB,GPIO_Pin_8);						 //待定
    }
}


