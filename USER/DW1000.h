//TODO : disable PAN
#define _PAN_ID 0x1074		//���˾�����id
#define _TX_sADDR 0x2014	//���ͷ��̵�ַ
#define _RX_sADDR 0x2015	//���շ��̵�ַ
#define _POLLING_FLAG 0x38  //��λ������֤�ֽ�
#define _WAVE_SPEED 299792458 //��Ų������ٶ�

#ifdef TX
void Location_polling(void);
void distance_measurement(void);
void quality_measurement(void);
#endif

#ifdef RX
void data_response(void);
#endif
void DW1000_init(void);
void RX_mode_enable(void);
void to_IDLE(void);
void ACK_send(void);
