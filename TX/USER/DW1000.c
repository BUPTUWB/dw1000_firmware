#include "stm32f10x.h"
#include "SPI.h"
#include "USART.h"
#include "DW1000.h"
#include "math.h"
#include "delay.h"
u8 toggle = 1;
const u16 PANIDS[2] = {0x8974, 0x1074};

u8 mesurement_done[2] = {0, 0};
float reciever_position[2][2] = {{0,0}, {0,0}};
float pseudolites_position[2][2] = {{0, 0}, {5.5, 0}};
float distance1;

u8 Sequence_Number=0x00;
extern u8 distance_flag;
u32 time_offset=0; //��Ų�����ʱ�����
u8 speed_offset=0; //��Ų������ٶȵ���
u32 Tx_stp_L;
u8 Tx_stp_H;
u32 Rx_stp_L;
u8 Rx_stp_H;
u32 data;
u32 tmp1;
s32 tmp2;
double diff;
double distance;
extern u8 ars_counter;
extern u8 tmpp[14];
u8 Tx_Buff[12];

u16 std_noise;
u16 fp_ampl1;
u16 fp_ampl2;
u16 fp_ampl3;
u16 cir_mxg;
u16 rxpacc;
double fppl;
double rxl;
/*
DW1000��ʼ��
*/


float sgn(float x) { return x >= 0 ? 1.0 : -1.0; }

void solve_2d(float reciever[2][2], float pseudolites[2][2], float pranges1, float pranges2) {
volatile float origin[2];
volatile float len;
volatile float tan_theta;
volatile float sin_theta;
volatile float cos_theta;
volatile float d1;
volatile float h1;
volatile float invrotation[2][2];
volatile float pranges[2];

  printf("\nPseudolites\n%f %f %f %f\n", pseudolites[0][0], pseudolites[0][1], pseudolites[1][0], pseudolites[1][1]);
  printf("\npr1 %f pr2 %f\n", pranges1, pranges2);

  pranges[0] = pranges1;
  pranges[1] = pranges2;

  origin[0] = pseudolites[0][0];
  origin[1] = pseudolites[0][1];

  pseudolites[0][0] = 0;
  pseudolites[0][1] = 0;
  pseudolites[1][0] = pseudolites[1][0] - origin[0];
  pseudolites[1][1] = pseudolites[1][1] - origin[1];

  len = sqrt(pow(pseudolites[1][0], 2) + pow(pseudolites[1][1], 2));

  tan_theta = pseudolites[1][1] / pseudolites[1][0];
  cos_theta = sgn(pseudolites[1][0]) / sqrt(pow(tan_theta, 2) + 1);
  sin_theta = sgn(pseudolites[1][1]) * fabs(tan_theta) / sqrt(pow(tan_theta, 2) + 1);

  invrotation[0][0] = cos_theta;
  invrotation[0][1] = -sin_theta;
  invrotation[1][0] = sin_theta;
  invrotation[1][1] = cos_theta;

  d1 = ((pow(pranges[0], 2) - pow(pranges[1], 2)) / len + len) / 2;

  h1 = sqrt(pow(pranges[0], 2) - pow(d1, 2));

  reciever[0][0] = d1;
  reciever[0][1] = h1;
  reciever[1][0] = d1;
  reciever[1][1] = -h1;
 
  reciever[0][0] = invrotation[0][0] * d1 + invrotation[0][1] * h1;
  reciever[0][1] = invrotation[1][0] * d1 + invrotation[1][1] * h1;
  reciever[0][0] += origin[0];
  reciever[0][1] += origin[1];

  reciever[1][0] = invrotation[0][0] * d1 + invrotation[0][1] * -h1;
  reciever[1][1] = invrotation[1][0] * d1 + invrotation[1][1] * -h1;
  reciever[1][0] += origin[0];
  reciever[1][1] += origin[1];
}

void DW1000_init(void)
{
	u32 tmp;
	////////////////////����ģʽ����////////////////////////
	//AGC_TUNE1	������Ϊ16 MHz PRF
	tmp=0x00008870;
	Write_DW1000(0x23,0x04,(u8 *)(&tmp),2);
	//AGC_TUNE2	 ����֪����ɶ�ã������ֲ���ȷ�涨Ҫд0x2502A907
	tmp=0x2502A907;
	Write_DW1000(0x23,0x0C,(u8 *)(&tmp),4);	
	//DRX_TUNE2������ΪPAC size 8��16 MHz PRF
	tmp=0x311A002D;
	Write_DW1000(0x27,0x08,(u8 *)(&tmp),4);
	//NSTDEV  ��LDE�ྶ���������㷨���������
	tmp=0x0000006D;
	Write_DW1000(0x2E,0x0806,(u8 *)(&tmp),1);
	//LDE_CFG2	����LDE�㷨����Ϊ��Ӧ16MHz PRF����
	tmp=0x00001607;
	Write_DW1000(0x2E,0x1806,(u8 *)(&tmp),2);
	//TX_POWER	 �������͹�������Ϊ16 MHz,���ܹ��ʵ���ģʽ 
	tmp=0x0E082848;
	Write_DW1000(0x1E,0x00,(u8 *)(&tmp),4);
	//RF_TXCTRL	 ��ѡ����ͨ��5
	tmp=0x001E3FE0;
	Write_DW1000(0x28,0x0C,(u8 *)(&tmp),4);
	//TC_PGDELAY�����������ʱ����Ϊ��ӦƵ��5
	tmp=0x000000C0;
	Write_DW1000(0x2A,0x0B,(u8 *)(&tmp),1);
	//FS_PLLTUNE   ��PPL����Ϊ��ӦƵ��5
	tmp=0x000000A6;
	Write_DW1000(0x2B,0x0B,(u8 *)(&tmp),1);
	/////////////////////ʹ�ù�������/////////////////////////
	//local address	��д�뱾����ַ��PAN_ID �ͱ����̵�ַ��
	tmp=PANIDS[toggle];
	tmp=(tmp<<16)+_TX_sADDR; 	
	Write_DW1000(0x03,0x00,(u8 *)(&tmp),4);  	  
	//re-enable	  auto ack	 Frame Filter �����������Զ��������ܡ��Զ�Ӧ���ܡ�֡���˹���
	tmp=0x200011FD;
	Write_DW1000(0x04,0x00,(u8 *)(&tmp),4);
	//  test pin SYNC�����ڲ��Ե�LED�����ų�ʼ����SYNC���Ž���	
	tmp=0x00101540;
	Write_DW1000(0x26,0x00,(u8 *)(&tmp),2);
	tmp=0x01;
	Write_DW1000(0x36,0x28,(u8 *)(&tmp),1);
	// interrupt   ���жϹ���ѡ��ֻ�����շ��ɹ��жϣ�
	tmp=0x00006080;
	Write_DW1000(0x0E,0x00,(u8 *)(&tmp),2);
	// ack�ȴ�
	tmp=3;
	Write_DW1000(0x1A,0x03,(u8 *)(&tmp),1);

	printf("��λоƬ����\t\t���\r\n");	
}
/*
���붨λ
*/
void Location_polling(void)	 //���Ͷ�λ֡
{
	u8 tmp;
									
	distance_flag=0;
	//��ַ���������� �����ֽ���ǰ ���ֽ�����д�룩
	Tx_Buff[0]=0x41;
	Tx_Buff[1]=0x88;
	Tx_Buff[2]=Sequence_Number++;	                //�����ڼ�������
	Tx_Buff[4]=PANIDS[toggle]>>8;
   	Tx_Buff[3]=(0x74);//MAC_maker((u8)_PAN_ID);
	Tx_Buff[6]=(0x20);//MAC_maker((u8)(_RX_sADDR>>8));
	Tx_Buff[5]=(0x15);//MAC_maker((u8)_RX_sADDR);
	Tx_Buff[8]=0x20;//MAC_maker((u8)(_TX_sADDR>>8));
	Tx_Buff[7]=0x14;//MAC_maker((u8)_TX_sADDR);
   	Tx_Buff[9]=_POLLING_FLAG;
	Tx_Buff[10]=0x12;
	Tx_Buff[11]=0x34;
	
	to_IDLE();
	Write_DW1000(0x09,0x00,Tx_Buff,12);
	tmp=14;
	Write_DW1000(0x08,0x00,&tmp,1);		//���ó���
	//Read_DW1000(0x08,0x00,&tmp,1);
	//printf("%2x\r\n",tmp);
	tmp=0x82;						//������ɺ�����ת��Ϊ����״̬
	Write_DW1000(0x0D,0x00,&tmp,1);

	//����������TIM3
	TIM_ClearFlag(TIM3, TIM_FLAG_Update);					    		
    TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);
	TIM_Cmd(TIM3, ENABLE);
	
}
/*
�򿪽���ģʽ
*/
void RX_mode_enable(void)
{
	u16 tmp;
	tmp=0x00;
	Write_DW1000(0x36,0x06,(u8 *)(&tmp),1);	
	//ROM TO RAM
	 //LDE LOAD=1
	tmp=0x8000;
	Write_DW1000(0x2D,0x06,(u8 *)(&tmp),2);	  
	Delay(20);
	tmp=0x0002;
	Write_DW1000(0x36,0x06,(u8 *)(&tmp),1);
	tmp=0x0001;
	Write_DW1000(0x0D,0x01,(u8 *)(&tmp),1);	
}
/*
����IDLE״̬
*/
void to_IDLE(void)
{
	u8 tmp;
	tmp=0x40;
	Write_DW1000(0x0D,0x00,&tmp,1);	
}
/*
���������Ϣ(��λ��cm)���������
*/
void distance_measurement(void)
{
	u32 tmp;
	toggle = !toggle;
	//printf("Toggled\n");
	tmp=PANIDS[toggle];
	tmp=(tmp<<16)+_TX_sADDR; 	
	Write_DW1000(0x03,0x00,(u8 *)(&tmp),4); 
	
	if(distance_flag!=3)
	{
		printf("�ⶨ����\t\t��������\r\n");		
	}
	else
	{
		if(Tx_stp_H==Rx_stp_H)
		{
			diff=1.0*(Rx_stp_L-Tx_stp_L);
		}
		else if(Tx_stp_H<Rx_stp_H)
		{
			diff=1.0*((Rx_stp_H-Tx_stp_H)*0xFFFFFFFF+Rx_stp_L-Tx_stp_L);
		}
		else
		{
			diff=1.0*((0xFF-Tx_stp_H+Rx_stp_H+1)*0xFFFFFFFF+Rx_stp_L-Tx_stp_L);
		}
		tmp2&=0x0007FFFF;
		tmp2*=(2^13);

		//diff-=(1.0-1.0*tmp2/tmp1/(2^13))*data;
		diff=diff-1.0*data ;
		//diff-=((double)time_offset);
		distance=15.65*diff/1000000000000/2*_WAVE_SPEED*(1.0-0.01*speed_offset);
		printf("�ⶨ����\t\t%.2lf��\r\n\n\n",distance-(toggle?148.63:150.13));

		if (distance > 148.93 && distance < 200.0){
			if (toggle == 0) {
				mesurement_done[0] = 1;
				distance1 = distance - 150.13;
				
			} else if (toggle == 1 && mesurement_done[0] == 1) {
				mesurement_done[0] = 0;
				printf("\nd1-------------- %f\n", distance1 - 148.63);
				solve_2d(reciever_position, pseudolites_position, distance1, distance - 148.63);
				printf("Position: %f %f", reciever_position[0][0], reciever_position[0][1]);
			}
		}
			
		printf("\r\n=====================================\r\n");
			
	}
}

/*
������������
*/
void quality_measurement(void)
{
	
		

	rxpacc>>=4;

	//������Ʒ���ж�
	if((fp_ampl2/std_noise)>=2)
	{
		//printf("������Ʒ��\t\t����\r\n");
	}
	else
	{
		//printf("������Ʒ��\t\t�쳣\r\n");
	}
	//LOS�ж�
	fppl=10.0*log((fp_ampl1^2+fp_ampl2^2+fp_ampl3^2)/(rxpacc^2))-115.72;
	rxl=10.0*log(cir_mxg*(2^17)/(rxpacc^2))-115.72;
	if((fppl-rxl)>=10.0*log(0.25))
	{
		//printf("LOS�ж�\t\t\tLOS\r\n");
	}
	else
	{
		//printf("LOS�ж�\t\t\tNLOS\r\n");
	}
}
void ACK_send(void)
{
	u8 tmp;
	

	Tx_Buff[0]=0x44;
	Tx_Buff[1]=0x00;
	Tx_Buff[2]=tmpp[2];

	to_IDLE();
	tmp=5;
	Write_DW1000(0x08,0x00,&tmp,1);
	Write_DW1000(0x09,0x00,Tx_Buff,3);
	tmp=0x82;
	Write_DW1000(0x0D,0x00,&tmp,1);
	

}
