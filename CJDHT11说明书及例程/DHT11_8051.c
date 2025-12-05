/****************************************************************
	   DHT11.C
       AT89S52  STC89C52RC 
2022.5.18:修改程序，读DHT11数据
****************************************************************/
#include <C8051F020.h>
#include <DST11.h>
extern wendu,shidu;

//----------------IO定义--------------------    //
#define   s_data P4|=0X08;
#define   c_data P4&=0XF7;

#define  P43 = P4&0x08;
//----------------------------------------------//
//  定义变量
//----------------------------------------------//
unsigned char  U8count,U8temp;
unsigned char  U8T_data_H_temp,U8T_data_L_temp,U8RH_data_H_temp,U8RH_data_L_temp,U8checkdata_temp;
//--------------------------------
//  延时100us ，已经校正！ 11.59MHz系统时钟
//--------------------------------
void Delay(unsigned int j)
    {   
	    unsigned char i;
	    for(;j>0;j--)
	   { 	
		   for(i=0;i<190;i++);
	    }
	}
//--------------------------------------------
//  读1个字节数据
//  这个读数和时序配合有关系
//--------------------------------------------
unsigned char  read_data()
     {   unsigned char low_period,high_period; //用于计算电平时间
         unsigned char i,temp;
	
		temp=0;
        for(i=0;i<8;i++)	   
	    {	
		     low_period = 0;
		     high_period =0;

			do{
				low_period++;
			}while((!(P4&0x08))&&(low_period));//检测低电平宽度
		
			do{
				high_period++;
			}while(((P4&0x08))&&(high_period));//检测高电平宽度

	    	// 判断数据位是0还是1?   high_period >= low_period 为1，否则 0；
			temp<<=1;
			if(high_period >=low_period)
			{
				 temp|=0x01;  
			}
	     }  
		 return temp;
	}
//--------------------------------
//  读温度、湿度
//--------------------------------
void read_wendu_shidu()
	{
		unsigned int counter;
	    unsigned int counter1;
        unsigned int counter2;
	   // ----主机拉低18ms-------
        c_data;              //主设备将 Pdata=0;
	    Delay(180);          //延时18mS
	   // ----主机设置为输入---------
	     s_data;             //主设备将 Pdata=1; 释放总线    
	 //等待切换i/o状态后的第一个高电平结束

		  counter =0;
		do{ counter++;	   
		  }while((P4&0x08)&&(counter));
       //判断从机发出80uS的低电平是否结束？
		 counter1 = 0;
		do{	counter1++;	
		   }while(!(P4&0x08)&&(counter1));	 
		  counter2 = 0;
        //判断从机发出80uS的高电平是否结束？	
		do{counter2++;
		  }while((P4&0x08)&&(counter2));           
     

		   //数据接收状态
          // test_time();//产生时间信号
	       U8RH_data_H_temp=read_data();
	      //湿度H

	       //test_time();//产生时间信号		   
		   U8RH_data_L_temp=read_data();
	       //湿度L
	       
		  // test_time();//产生时间信号		   
		   U8T_data_H_temp=read_data();
	       //温度H	        
           
		   //test_time();//产生时间信号		  
		   U8T_data_L_temp=read_data();
	       //温度L	         
		   
		  // test_time();//产生时间信号		   
		   U8checkdata_temp=read_data();	       
		   //效验和       

		   s_data;

	      //数据效验和处理	 
	       U8temp=(U8T_data_H_temp+U8T_data_L_temp+U8RH_data_H_temp+U8RH_data_L_temp);
	       if(U8temp==U8checkdata_temp)
	       {
              
	   	      shidu=U8RH_data_H_temp*100+U8RH_data_L_temp;
              if((U8T_data_L_temp&0x80)==0)
			  {
	   	           wendu=U8T_data_H_temp*100+U8T_data_L_temp;	
	     	    }
				else{
				       wendu=U8T_data_H_temp*100+U8T_data_L_temp&0x7f;
	 				   wendu*=-1;
				     }
	        }
	  }
//--------------------------------------

