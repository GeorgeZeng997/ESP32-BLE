#include "DHT11.h"
#include "MAIN.h"
/*
 * 函数名：DHT11_GPIO_Config
 * 描述  ：配置DHT11用到的I/O口  PA.5
 * 输入  ：无
 * 输出  ：无
 */
 void delay(int tt)
 {	   int j;
    for(;tt!=0;tt--)
		for(j=44;j>0;j--);
 }
void DHT11_GPIO_Config(void)
{		
	/*定义一个GPIO_InitTypeDef类型的结构体*/
	GPIO_InitTypeDef GPIO_InitStructure;

	/*开启GPIOD的外设时钟*/
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE); 

	/*选择要控制的GPIOD引脚*/															   
  	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;	

	/*设置引脚模式为通用推挽输出*/
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   

	/*设置引脚速率为50MHz */   
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 

	/*调用库函数，初始化GPIOD*/
  	GPIO_Init(GPIOA, &GPIO_InitStructure);		  

	/* 拉高GPIOD12	*/
	GPIO_SetBits(GPIOA, GPIO_Pin_5);
 
}

/*
 * 函数名：DHT11_Mode_IPU
 * 描述  ：使DHT11-DATA引脚变为输入模式
 * 输入  ：无
 * 输出  ：无
 */
static void DHT11_Mode_IPU(void)
{
 	  GPIO_InitTypeDef GPIO_InitStructure;

	  	/*选择要控制的GPIOD引脚*/	
	  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;

	   /*设置引脚模式为浮空输入模式*/ 
	  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU ; 

	  /*调用库函数，初始化GPIOD*/
	  GPIO_Init(GPIOA, &GPIO_InitStructure);	 
}

/*
 * 函数名：DHT11_Mode_Out_PP
 * 描述  ：使DHT11-DATA引脚变为输出模式
 * 输入  ：无
 * 输出  ：无
 */
static void DHT11_Mode_Out_PP(void)
{
 	GPIO_InitTypeDef GPIO_InitStructure;

	 	/*选择要控制的GPIOD引脚*/															   
  	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;	

	/*设置引脚模式为通用推挽输出*/
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   

	/*设置引脚速率为50MHz */   
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	/*调用库函数，初始化GPIOD*/
  	GPIO_Init(GPIOA, &GPIO_InitStructure);	 	 
}
static uint8_t Read_Byte(void)
{	  

   	 uint8_t i, temp=0;


	 for(i=0;i<8;i++)    
	 {
		 uint8_t low_time=0,high_time=0;
	
      do		 
     {
			 ++low_time;
		 }while((DHT11_DATA_IN()==Bit_RESET)&&(low_time !=0));// 计算低电平时间，超时退出
		 
		 do
     {
			 ++high_time;
		 }while((DHT11_DATA_IN()==Bit_SET)&&(high_time !=0));// 计算高电平时间，超时退出
		 
		 temp=temp*2;              //接收数据左移一位
     if(high_time>low_time)    // 判断计算高电平时间大于低电平宽度
		 {
       temp+=1;                 //若大于，则收到'1'
     } 

	 }
	  return temp;

}

uint8_t Read_DHT11(DHT11_Data_TypeDef *DHT11_Data)
{  
	uint8_t temp=0;
	/*输出模式*/

   DHT11_Mode_Out_PP();
   /*主机拉低*/
   DHT11_DATA_OUT(LOW);
   //*延时18ms*/800US
   SysTickDelay(18000); 

   
 	/*主机设为输入 判断从机响应信号*/ 
   DHT11_Mode_IPU();
   while((DHT11_DATA_IN()==Bit_SET)&&((++temp)!=0)); //忽略拉高等待高电平，超时退出
	
	 temp=0;
	
	 while((DHT11_DATA_IN()==Bit_RESET)&&((++temp)!=0));//忽略响应信号，超时退出
	
	 temp=0;
	
   while((DHT11_DATA_IN()==Bit_SET)&&((++temp)!=0));//忽略拉高准备输出高电平，超时退出
	 
	 	 DHT11_Data->humi_int= Read_Byte();	   //读取湿度整数部分
								
		 DHT11_Data->humi_deci= Read_Byte();	  //读取湿度小数部分 0；
				
		 DHT11_Data->temp_int= Read_Byte();			 //读取温度整数部分

		 DHT11_Data->temp_deci= Read_Byte();			//读取温度小数部分
				 		
		 DHT11_Data->check_sum= Read_Byte();	    //读取校验码
		 if(DHT11_Data->check_sum == (uint8_t)
			 (DHT11_Data->humi_int + DHT11_Data->humi_deci + DHT11_Data->temp_int+ DHT11_Data->temp_deci))// 检查数据是否正确
		  	{
				   return SUCCESS;  //如正确，返回成功值
			  }
		  else 
		  	return ERROR;  		 	//数据错误，返回错误 	
}

	  


/*************************************END OF FILE******************************/
