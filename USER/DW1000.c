#include "stm32f10x.h"
#include "SPI.h"
#include "DW1000.h"
#include "USART.h"
#include "math.h"
#include "delay.h"
#include "utils.h"
#include "solve.h"

#include "CONFIG.h"

u8 mac[8];
u8 emac[6];
u8 toggle = 1;
const u8 broadcast_addr[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
extern u32 data[16];
// Common
u8 Sequence_Number = 0x00;
u32 Tx_stp_L;
u8 Tx_stp_H;
u32 Rx_stp_L;
u8 Rx_stp_H;
u32 Tx_diff;

u32 Tx_stp_LT[3];
u8 Tx_stp_HT[3];
u32 Rx_stp_LT[3];
u8 Rx_stp_HT[3];

u8 Tx_Buff[128];
u8 Rx_Buff[128];
u32 LS_DATA[3];
u32 LS_DELAY[3];
u32 u32_diff;
extern xyz anchors[3];

extern u8 distance_flag;

u32 time_offset = 0; //鐢电����娉����紶鎾����椂闂磋皟鏁�
u8 speed_offset = 0; //鐢电����娉����紶鎾�����熷害璋冩暣

float raw_distance[3];
float calib[3];
float distance[3];
xyz location;
extern float calib[3];

u16 std_noise;
u16 fp_ampl1;
u16 fp_ampl2;
u16 fp_ampl3;
u16 cir_mxg;
u16 rxpacc;
double fppl;
double rxl;

extern int debug_lvl;

u8 status_flag = IDLE;
u8 distance_flag = IDLE;

int polling_counter = 0;

// 瀛樺偍鐢眊et_antenna_delay(dip_config)寰楀嚭鐨刟ntenna_delay
int antenna_delay;

/*
DW1000鍒濆����鍖�
*/

void DW1000_init(u8 dip_config) {
	u32 tmp;
	u32 status;
	int i;

	__disable_irq();
	#ifdef TX
	antenna_delay = TX_ANTENNA_DELAY;
	#endif
	#ifdef RX
	antenna_delay = get_antenna_delay(dip_config);
	#endif

	DW1000_trigger_reset();

	////////////////////宸ヤ綔妯″紡閰嶇疆////////////////////////
	//lucus
	//Channel Control PCODE 4 CHAN 5
	tmp = 0x21040055;
	Write_DW1000(0x1F, 0x00, (u8 *)(&tmp), 4);
	//AGC_TUNE1 锛氳����缃����负16 MHz PRF
	tmp = 0x00008870;
	Write_DW1000(0x23, 0x04, (u8 *)(&tmp), 2);
	//AGC_TUNE2 锛氫笉鐭ラ亾骞插暐鐢����紝鎶�鏈����墜鍐屾槑纭��������瀹氳����鍐�0x2502A907
	tmp = 0x2502A907;
	Write_DW1000(0x23, 0x0C, (u8 *)(&tmp), 4);
	//DRX_TUNE2锛氶厤缃����负PAC size 8锛�16 MHz PRF
	tmp = 0x311A002D;
	Write_DW1000(0x27, 0x08, (u8 *)(&tmp), 4);
	//NSTDEV 锛歀DE澶氬緞骞叉壈娑堥櫎绠楁硶鐨勭浉鍏抽厤缃�
	tmp = 0x0000006C;
	Write_DW1000(0x2E, 0x0806, (u8 *)(&tmp), 1);
	//LDE_CFG2 锛氬皢LDE绠楁硶閰嶇疆涓洪�傚簲16MHz PRF鐜��������
	tmp = 0x00001607;
	Write_DW1000(0x2E, 0x1806, (u8 *)(&tmp), 2);
	//TX_POWER 锛氬皢鍙戦�佸姛鐜囬厤缃����负16 MHz,鏅鸿兘鍔熺巼璋冩暣妯″紡
	tmp = 0x0E082848;
	Write_DW1000(0x1E, 0x00, (u8 *)(&tmp), 4);
	//RF_TXCTRL 锛氶�夋嫨鍙戦�侀�氶亾5
	tmp = 0x001E3FE0;
	Write_DW1000(0x28, 0x0C, (u8 *)(&tmp), 4);
	//lucus
	//RF_RXCTRLH CHAN 5
	tmp = 0x000000D8;
	Write_DW1000(0x28, 0x0B, (u8 *)(&tmp), 1);
	//lucus
	//LDE_REPC PCODE 4
	tmp = 0x0000428E;
	Write_DW1000(0x2E, 0x2804, (u8 *)(&tmp), 2);
	//TC_PGDELAY 锛氳剦鍐蹭骇鐢熷欢鏃惰����缃����负閫傚簲棰戦亾5
	tmp = 0x000000C0;
	Write_DW1000(0x2A, 0x0B, (u8 *)(&tmp), 1);
	//FS_PLLTUNE 锛歅PL璁剧疆涓洪�傚簲棰戦亾5
	tmp = 0x000000A6;
	Write_DW1000(0x2B, 0x0B, (u8 *)(&tmp), 1);
	load_LDE();
	mac[0] = 0xff;
	mac[1] = 0xff;
	mac[2] = 0xff;
	mac[3] = 0xff;
	mac[4] = 0xff;
	mac[5] = 0xff;
	mac[6] = 0xff;

	#ifdef TX
	mac[7] = 0x00;
	#endif

	#ifdef RX
	mac[7] = 0xf0 | (0x0f & dip_config);
	#endif

	set_MAC(mac);
	//no auto ack Frame Filter
	tmp = 0x200011FC;
	// 0010 0000 0000 0001 0000 0111 1101
	Write_DW1000(0x04, 0x00, (u8 *)(&tmp), 4);
	// test pin SYNC锛氱敤浜庢祴璇曠殑LED鐏����紩鑴氬垵濮嬪寲锛孲YNC寮曡剼绂佺敤
	tmp = 0x00101540;
	Write_DW1000(0x26, 0x00, (u8 *)(&tmp), 2);
	tmp = 0x01;
	Write_DW1000(0x36, 0x28, (u8 *)(&tmp), 1);
	// interrupt   锛氫腑鏂����姛鑳介�夋嫨锛堝彧寮�鍚����敹鍙戞垚鍔熶腑鏂����級
	tmp = 0x00002080;
	Write_DW1000(0x0E, 0x00, (u8 *)(&tmp), 2);
	// ack绛夊緟
	tmp = 3;
	Write_DW1000(0x1A, 0x03, (u8 *)(&tmp), 1);
	for(i = 0; i < 10; i++)
		Delay();
	load_LDE();
	load_LDE();
	load_LDE();
	load_LDE();
	load_LDE();
	read_status(&status);
	DEBUG2(("DW1K Setup\t\tFinished, with Status: 0x%08X, Mac: 0x%02X.\r\n", status, mac[7]));

	__enable_irq();
}

/*
鐢宠����瀹氫綅
*/
#ifdef TX
void Location_polling(void) {
	u16 tmp;
	// Tx_Buff[0]=0b10000010; // only DST PANID
	// Tx_Buff[1]=0b00110111;
	static u8 count = 0;

	// Polling interval handling
	count += 1;
	switch (count) {
	case 1:
	case 2:
	case 3:
		// when count == 1/2/3, perform polling
		#ifdef FAKE_SERIAL
		return;
		#else
		break;
		#endif
	case 4:
		//when count == 4, upload results
		#ifdef SOLVE_LOCATION
		location = solve_3d(anchors, distance);
		#endif
		upload_location_info();
		status_forward();
		return;
	case TICK_IN_PERIOD:
		// when count exceeds TICK_IN_PERIOD, zero it
		count = 0;
		DEBUG2(("\r\n"));
		return;
	default:
		// otherwise, do nothing
		DEBUG2(("."));
		return;
	}

	Tx_Buff[0] = 0x82;
	Tx_Buff[1] = 0x37;
	Tx_Buff[2] = Sequence_Number++;
	//SN end
	Tx_Buff[4] = 0xFF;
	Tx_Buff[3] = 0xFF;
	//DST PAN end
	Tx_Buff[6] = broadcast_addr[0];
	Tx_Buff[7] = broadcast_addr[1];
	Tx_Buff[8] = broadcast_addr[2];
	Tx_Buff[9] = broadcast_addr[3];
	Tx_Buff[10] = broadcast_addr[4];
	Tx_Buff[11] = broadcast_addr[5];
	Tx_Buff[12] = broadcast_addr[6];
	Tx_Buff[13] = 0xF0 | count; // count could only be 1/2/3
	// Tx_Buff[13]=broadcast_addr[7];
	//DST MAC end
	Tx_Buff[14] = mac[0];
	Tx_Buff[15] = mac[1];
	Tx_Buff[16] = mac[2];
	Tx_Buff[17] = mac[3];
	Tx_Buff[18] = mac[4];
	Tx_Buff[19] = mac[5];
	Tx_Buff[20] = mac[6];
	Tx_Buff[21] = mac[7];
	//SRC MAC end
	//NO AUX
	//Payload begin
	Tx_Buff[22] = 0x00; // 0x00 = LS Req
	Tx_Buff[23] = 0xFF;
	tmp = 23;
	raw_write(Tx_Buff, &tmp);
	distance_flag = SENT_LS_REQ;
}
#endif

/*
璁＄畻璺濈����淇℃伅(鍗曚綅锛歝m)
*/
void distance_measurement(int n) {
	u32 double_diff;
	u32 rxtx_antenna_delay;
	int net_time_of_flight;

	rxtx_antenna_delay = ((u32) (antenna_delay) + (u32) (LS_DELAY[n]));

	DEBUG3(("ANTENNA_DELAY_THIS: %d", antenna_delay));
	DEBUG3(("ANTENNA_DELAY_THAT[%d]: %d\r\n", n, LS_DELAY[n]));

	if (Tx_stp_H == Rx_stp_HT[n]) {
		double_diff = (Rx_stp_LT[n] - Tx_stp_L);
	} else if (Tx_stp_H < Rx_stp_HT[n]) {
		double_diff = ((Rx_stp_HT[n] - Tx_stp_H) * 0xFFFFFFFF + Rx_stp_LT[n] - Tx_stp_L);
	} else {
		double_diff = ((0xFF - Tx_stp_H + Rx_stp_HT[n] + 1) * 0xFFFFFFFF + Rx_stp_LT[n] - Tx_stp_L);
	}

	DEBUG3(("rxtx_antenna_delay: %d\r\n", rxtx_antenna_delay));
	DEBUG3(("LS_DATA[n] : %d\r\n", LS_DATA[n]));
	DEBUG3(("double_diff: %d\r\n", double_diff));

	if (debug_lvl > 0) {
		raw_distance[n] = 15.65 / 1000000000000 * (float) (double_diff - LS_DATA[n]) / 2 * _WAVE_SPEED;
	}

	net_time_of_flight = double_diff - 2 * rxtx_antenna_delay - LS_DATA[n];
	DEBUG3(("after_diff : %d\r\n", net_time_of_flight));
	net_time_of_flight = (net_time_of_flight > 0) ? net_time_of_flight : 0;

	// distance[n] = 1.0*_WAVE_SPEED * (1.0*Tx_diff - 1.0*LS_DATA[n]) / (128.0 * 499.2 * 1000000.0);

	distance[n] = 15.65 / 1000000000000 * (float) (net_time_of_flight) / 2 * _WAVE_SPEED;
	// 4.6917519677e-3 * net_time_of_flight / 2

	#ifdef RX4
	data[0] = (u32)(100 * distance[0]);
	data[1] = (u32)(100 * distance[1]);
	data[2] = (u32)(100 * distance[2]);
	#endif
}

//void location_info_upload(void) {
//        int i;
//        u8 upload_buffer[64] ;
//        printf("#|\n");
//        upload_buffer[0]  = 0x80;
//        upload_buffer[1]  = (u8) 12;
//        upload_buffer[2]  = (data[0] >> 24) | 0x00;
//        upload_buffer[3]  = (data[0] >> 16) | 0x00;
//        upload_buffer[4]  = (data[0] >> 8)  | 0x00;
//        upload_buffer[5]  = (data[0])       | 0x00;
//        upload_buffer[6]  = (data[1] >> 24) | 0x00;
//        upload_buffer[7]  = (data[1] >> 16) | 0x00;
//        upload_buffer[8]  = (data[1] >> 8)  | 0x00;
//        upload_buffer[9]  = (data[1])       | 0x00;
//        upload_buffer[10] = (data[2] >> 24) | 0x00;
//        upload_buffer[11] = (data[2] >> 16) | 0x00;
//        upload_buffer[12] = (data[2] >> 8)  | 0x00;
//        upload_buffer[13] = (data[2])       | 0x00;
//        for (i = 0; i < 64; i++) {
//                printf("%s", upload_buffer);
//        }
//        printf("|#\n");
//}

void status_forward(void) {
	u16 tmp;
	// Tx_Buff[0]=0b10000010; // only DST PANID
	// Tx_Buff[1]=0b00110111;
	Tx_Buff[0] = 0x82;
	Tx_Buff[1] = 0x37;
	Tx_Buff[2] = Sequence_Number; // HHHHHHHHHHHEAR remove ++
	//SN end
	Tx_Buff[4] = 0xFF;
	Tx_Buff[3] = 0xFF;
	//DST PAN end
	Tx_Buff[6] = broadcast_addr[0];
	Tx_Buff[7] = broadcast_addr[1];
	Tx_Buff[8] = broadcast_addr[2];
	Tx_Buff[9] = broadcast_addr[3];
	Tx_Buff[10] = broadcast_addr[4];
	Tx_Buff[11] = broadcast_addr[5];
	Tx_Buff[12] = broadcast_addr[6];
	Tx_Buff[13] = 0xF0; // for RX0

	// Tx_Buff[13]=broadcast_addr[7];
	//DST MAC end
	Tx_Buff[14] = mac[0];
	Tx_Buff[15] = mac[1];
	Tx_Buff[16] = mac[2];
	Tx_Buff[17] = mac[3];
	Tx_Buff[18] = mac[4];
	Tx_Buff[19] = mac[5];
	Tx_Buff[20] = mac[6];
	Tx_Buff[21] = mac[7];
	//SRC MAC end
	//NO AUX
	//Payload begin
	Tx_Buff[22] = 0x04;

	float_to_bytes(&(Tx_Buff[23]), location.x);
	float_to_bytes(&(Tx_Buff[27]), location.y);
	float_to_bytes(&(Tx_Buff[31]), location.z);

	float_to_bytes(&(Tx_Buff[35]), raw_distance[0]);
	float_to_bytes(&(Tx_Buff[39]), raw_distance[1]);
	float_to_bytes(&(Tx_Buff[43]), raw_distance[2]);

	float_to_bytes(&(Tx_Buff[47]), distance[0]);
	float_to_bytes(&(Tx_Buff[51]), distance[1]);
	float_to_bytes(&(Tx_Buff[55]), distance[2]);

	float_to_bytes(&(Tx_Buff[59]), calib[0]);
	float_to_bytes(&(Tx_Buff[63]), calib[1]);
	float_to_bytes(&(Tx_Buff[67]), calib[2]);

	u32_to_bytes(&(Tx_Buff[71]), LS_DATA[0]);
	u32_to_bytes(&(Tx_Buff[75]), LS_DATA[1]);
	u32_to_bytes(&(Tx_Buff[79]), LS_DATA[2]);

	Tx_Buff[80] = 0xFF;
	tmp = 80;
	raw_write(Tx_Buff, &tmp);
}

void handle_distance_forward(u8* payload) {
	data[0] = (payload[1] << 24) + (payload[2] << 16) + (payload[3] << 8) + (payload[4]);
	data[1] = (payload[5] << 24) + (payload[6] << 16) + (payload[7] << 8) + (payload[8]);
	data[2] = (payload[9] << 24) + (payload[10] << 16) + (payload[11] << 8) + (payload[12]);

	location.x = bytes_to_float(&(payload[1]));
	location.y = bytes_to_float(&(payload[5]));
	location.z = bytes_to_float(&(payload[9]));

	raw_distance[0] = bytes_to_float(&(payload[13]));
	raw_distance[1] = bytes_to_float(&(payload[17]));
	raw_distance[2] = bytes_to_float(&(payload[21]));

	distance[0] = bytes_to_float(&(payload[25]));
	distance[1] = bytes_to_float(&(payload[29]));
	distance[2] = bytes_to_float(&(payload[33]));

	calib[0] = bytes_to_float(&(payload[37]));
	calib[1] = bytes_to_float(&(payload[41]));
	calib[2] = bytes_to_float(&(payload[45]));

	LS_DATA[0] = bytes_to_u32(&(payload[49]));
	LS_DATA[1] = bytes_to_u32(&(payload[53]));
	LS_DATA[2] = bytes_to_u32(&(payload[57]));

	upload_location_info();
}

/*
鏃犵嚎璐ㄩ噺鏁版嵁
*/
void quality_measurement(void) {
	rxpacc >>= 4;

	//鎶楀櫔澹板搧璐ㄥ垽瀹�
	if((fp_ampl2 / std_noise) >= 2) {
		//printf("鎶楀櫔澹板搧璐╘t\t鑹����ソ\r\n");
	} else {
		//printf("鎶楀櫔澹板搧璐╘t\t寮傚父\r\n");
	}
	//LOS鍒ゅ畾
	fppl = 10.0 * log((fp_ampl1 ^ 2 + fp_ampl2 ^ 2 + fp_ampl3 ^ 2) / (rxpacc ^ 2)) - 115.72;
	rxl = 10.0 * log(cir_mxg * (2 ^ 17) / (rxpacc ^ 2)) - 115.72;
	if((fppl - rxl) >= 10.0 * log(0.25)) {
		//printf("LOS鍒ゅ畾\t\t\tLOS\r\n");
	} else {
		//printf("LOS鍒ゅ畾\t\t\tNLOS\r\n");
	}
}



void DW1000_trigger_reset(void) {
	GPIO_ResetBits(GPIOC, GPIO_Pin_5);
	delay(0xFFFF);
	GPIO_SetBits(GPIOC, GPIO_Pin_5);
	delay(0xFFFF);
}

/*
鎵撳紑鎺ユ敹妯″紡
*/
void RX_mode_enable(void) {
	u8 tmp;
	// load_LDE(); // lucus: why here?
	tmp = 0x01;
	Write_DW1000(0x0D, 0x01, &tmp, 1);
}
/*
杩斿洖IDLE鐘舵��
*/
void to_IDLE(void) {
	u8 tmp;
	tmp = 0x40;
	Write_DW1000(0x0D, 0x00, &tmp, 1);
}

void raw_write(u8* tx_buff, u16* size) {
	u8 full_size;
	to_IDLE();
	Write_DW1000(0x09, 0x00, tx_buff, *size);
	full_size = (u8)(*size + 2);
	Write_DW1000(0x08, 0x00, &full_size, 1);
	// sent and wait
	sent_and_wait();
}

void raw_read(u8* rx_buff, u16* size) {
	u8 full_size;
	to_IDLE();
	Read_DW1000(0x10, 0x00, &full_size, 1);
	*size = (u16)(full_size - 2);
	DEBUG2(("%d\r\n", *size));
	Read_DW1000(0x11, 0x00, rx_buff, *size);
	RX_mode_enable();
}

void load_LDE(void) {
	u16 tmp;
	int i;
	tmp = 0x0000;
	Write_DW1000(0x36, 0x06, (u8 *)(&tmp), 1);
	tmp = 0x8000;
	Write_DW1000(0x2D, 0x06, (u8 *)(&tmp), 2);
	for(i = 0; i < 100; i++)
		Delay();
	tmp = 0x0002;
	Write_DW1000(0x36, 0x06, (u8 *)(&tmp), 1);
	tmp = 0x0008;
	Write_DW1000(0x2C, 0x01, (u8 *)(&tmp), 1);
}

void sent_and_wait(void) {
	u8 tmp = 0x82;
	Write_DW1000(0x0D, 0x00, &tmp, 1);
}

void set_MAC(u8* mac) {
	Write_DW1000(0x01, 0x00, mac, 8);
	emac[0] = mac[2];
	emac[1] = mac[3];
	emac[2] = mac[4];
	emac[3] = mac[5];
	emac[4] = mac[6];
	emac[5] = mac[7];
}

void read_status(u32 *status) {
	Read_DW1000(0x0F, 0x00, (u8 *)(status), 4);
}

void send_LS_ACK(u8 *src, u8 *dst) {
	u16 tmp;
	// Tx_Buff[0]=0b10000010; // only DST PANID
	// Tx_Buff[1]=0b00110111;
	Tx_Buff[0] = 0x82;
	Tx_Buff[1] = 0x37;
	// 0100 0001 1000 1000
	Tx_Buff[2] = Sequence_Number++;
	//SN end
	Tx_Buff[4] = 0xFF;
	Tx_Buff[3] = 0xFF;
	//DST PAN end
	Tx_Buff[6] = dst[0];
	Tx_Buff[7] = dst[1];
	Tx_Buff[8] = dst[2];
	Tx_Buff[9] = dst[3];
	Tx_Buff[10] = dst[4];
	Tx_Buff[11] = dst[5];
	Tx_Buff[12] = dst[6];
	Tx_Buff[13] = dst[7];
	//DST MAC end
	Tx_Buff[14] = src[0];
	Tx_Buff[15] = src[1];
	Tx_Buff[16] = src[2];
	Tx_Buff[17] = src[3];
	Tx_Buff[18] = src[4];
	Tx_Buff[19] = src[5];
	Tx_Buff[20] = src[6];
	Tx_Buff[21] = src[7];
	//SRC MAC end
	Tx_Buff[22] = 0x01; // 0x01 = LS ACK
	Tx_Buff[23] = 0x01;
	tmp = 23;
	raw_write(Tx_Buff, &tmp);
}

void send_LS_DATA(u8 *src, u8 *dst) {
	u16 tmp;
	to_IDLE();
	Read_DW1000(0x17, 0x00, (u8 *)(&Tx_stp_L), 4);
	Read_DW1000(0x15, 0x00, (u8 *)(&Rx_stp_L), 4);
	Read_DW1000(0x17, 0x04, &Tx_stp_H, 1);
	Read_DW1000(0x15, 0x04, &Rx_stp_H, 1);
	DEBUG2(("===========Response DATA===========\r\n"));
	DEBUG2(("Rx_stp_L %8x\r\n", Rx_stp_L));
	DEBUG2(("Rx_stp_H %2x\r\n", Rx_stp_H));
	DEBUG2(("Tx_stp_L %8x\r\n", Tx_stp_L));
	DEBUG2(("Tx_stp_H %2x\r\n", Tx_stp_H));
	if(Tx_stp_H == Rx_stp_H) {
		u32_diff = (Tx_stp_L - Rx_stp_L);
	} else if(Rx_stp_H < Tx_stp_H) {
		u32_diff = ((Tx_stp_H - Rx_stp_H) * 0xFFFFFFFF + Tx_stp_L - Rx_stp_L);
	} else {
		u32_diff = ((0xFF - Rx_stp_H + Tx_stp_H + 1) * 0xFFFFFFFF + Tx_stp_L - Rx_stp_L);
	}

	DEBUG2(("u32_diff %08x\r\n", u32_diff));
	// Tx_Buff[0]=0b10000010; // only DST PANID
	// Tx_Buff[1]=0b00110111;
	Tx_Buff[0] = 0x82;
	Tx_Buff[1] = 0x37;
	// 0100 0001 1000 1000
	Tx_Buff[2] = Sequence_Number++;
	//SN end
	Tx_Buff[4] = 0xFF;
	Tx_Buff[3] = 0xFF;
	//DST PAN end
	Tx_Buff[6] = dst[0];
	Tx_Buff[7] = dst[1];
	Tx_Buff[8] = dst[2];
	Tx_Buff[9] = dst[3];
	Tx_Buff[10] = dst[4];
	Tx_Buff[11] = dst[5];
	Tx_Buff[12] = dst[6];
	Tx_Buff[13] = dst[7];
	//DST MAC end
	Tx_Buff[14] = src[0];
	Tx_Buff[15] = src[1];
	Tx_Buff[16] = src[2];
	Tx_Buff[17] = src[3];
	Tx_Buff[18] = src[4];
	Tx_Buff[19] = src[5];
	Tx_Buff[20] = src[6];
	Tx_Buff[21] = src[7];
	//SRC MAC end
	Tx_Buff[22] = 0x02; // 0x02 = LS DATA
	Tx_Buff[23] = (u8)u32_diff;
	u32_diff >>= 8;
	Tx_Buff[24] = (u8)u32_diff;
	u32_diff >>= 8;
	Tx_Buff[25] = (u8)u32_diff;
	u32_diff >>= 8;
	Tx_Buff[26] = (u8)u32_diff;
	Tx_Buff[27] = 0x01;

	// antenna delay
	Tx_Buff[28] = (u8)(antenna_delay >> 0);
	Tx_Buff[29] = (u8)(antenna_delay >> 8);
	Tx_Buff[30] = (u8)(antenna_delay >> 16);
	Tx_Buff[31] = (u8)(antenna_delay >> 24);
	Tx_Buff[32] = 0x01;

	tmp = 32;
	raw_write(Tx_Buff, &tmp);
}

void parse_rx(u8 *rx_buff, u16 size, u8 **src, u8 **dst, u8 **payload, u16 *pl_size) {
	u16 n = 24;
	if(rx_buff[0] & 0x02 == 0x02) {
		// PANID compress
		n -= 2;
	}

	if(rx_buff[1] & 0x30 == 0x00) {
		n -= 8;
	} else if(rx_buff[1] & 0x30 == 0x20) {
		n -= 6;
	}

	if(rx_buff[1] & 0x03 == 0x00) {
		n -= 8;
	} else if(rx_buff[1] & 0x03 == 0x02) {
		n -= 6;
	}

	*src = &(rx_buff[n - 8]);
	*dst = &(rx_buff[n - 16]);
	*payload = &(rx_buff[n]);
	*pl_size = (u16)(size - n);
}

int get_antenna_delay(u8 n) {
	switch (n & 0x0F) {
	case 0x01:
		return RX1_ANTENNA_DELAY;
	case 0x02:
		return RX2_ANTENNA_DELAY;
	case 0x03:
		return RX3_ANTENNA_DELAY;
	default:
		return -30000;
	}
}

void Read_VotTmp(u8 * voltage, u8 * temperature) {
	u8 tmp;
	tmp = 0x80;
	Write_DW1000(0x28, 0x11, &tmp, 1);
	tmp = 0x0a;
	Write_DW1000(0x28, 0x12, &tmp, 1);
	tmp = 0x0F;
	Write_DW1000(0x28, 0x12, &tmp, 1);
	tmp = 0x01;
	Write_DW1000(0x2A, 0x00, &tmp, 1);
	Delay();
	tmp = 0x00;
	Write_DW1000(0x2A, 0x00, &tmp, 1);

	Read_DW1000(0x2A, 0x03, voltage, 1);
	Read_DW1000(0x2A, 0x04, temperature, 1);
}

void Read_Tmp(u8 * temperature) {
	u8 tmp;
	tmp = 0x80;
	Write_DW1000(0x28, 0x11, &tmp, 1);
	tmp = 0x0A;
	Write_DW1000(0x28, 0x12, &tmp, 1);
	tmp = 0x0F;
	Write_DW1000(0x28, 0x12, &tmp, 1);

	// SAR
	tmp = 0x00;
	Write_DW1000(0x2A, 0x00, &tmp, 1);
	tmp = 0x01;
	Write_DW1000(0x2A, 0x00, &tmp, 1);

	Read_DW1000(0x2A, 0x04, temperature, 1);

	tmp = 0x00;
	Write_DW1000(0x2A, 0x00, &tmp, 1);
}

void Init_VotTmp(u8 * voltage, u8 * temperature) {
	u8 tmp;
	tmp = 0x80;
	Write_DW1000(0x28, 0x11, &tmp, 1);
	tmp = 0x0a;
	Write_DW1000(0x28, 0x12, &tmp, 1);
	tmp = 0x0F;
	Write_DW1000(0x28, 0x12, &tmp, 1);
	tmp = 0x01;
	Write_DW1000(0x2A, 0x00, &tmp, 1);
	Delay();
	tmp = 0x00;
	Write_DW1000(0x2A, 0x00, &tmp, 1);
}

u8 Read_DIP_Configuration(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	u8 dip_config = 0x00;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;

	// PC1-2-3, PA0-1-2-3-4 for DIP switch input
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	dip_config |= !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) << 7;
	dip_config |= !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) << 6;
	dip_config |= !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_3) << 5;
	dip_config |= !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) << 4;
	dip_config |= !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) << 3;
	dip_config |= !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) << 2;
	dip_config |= !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) << 1;
	dip_config |= !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);

	return dip_config;
}

void handle_event(void) {
	u32 status;
	u8 tmp;
	u16 size;
	// u16 pl_size;
	static u8 *dst;
	static u8 *src; // need improvement
	u8 *payload;
	int i;
	int for_me = 1;

	EXTI_ClearITPendingBit(EXTI_Line0);
	// enter interrupt
	while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0) {
		// printf("Int Triggered.\r\n");
		read_status(&status);
		DEBUG2(("status: %08X\r\n", status));

		if ((status & 0x00000400) == 0x00000400) { // LDE Success
			DEBUG2(("LDE Success.\r\n"));
			if((distance_flag == CONFIRM_SENT_LS_REQ) || (distance_flag == SENT_LS_REQ)) {
				Read_DW1000(0x15, 0x00, (u8 *)(&Rx_stp_L), 4);
				Read_DW1000(0x15, 0x04, &Rx_stp_H, 1);
			}
		} else if ((status & 0x00040000) == 0x00040000) { // LDE Err
			to_IDLE();
			tmp = 0x04;
			// Clear Flag
			Write_DW1000(0x0F, 0x02, &tmp, 1);
			DEBUG2(("LDE err.\r\n"));
			load_LDE();
			RX_mode_enable();
		}

		if((status & 0x0000C000) == 0x00008000) { // CRC err
			tmp = 0xF0;
			Write_DW1000(0x0F, 0x01, &tmp, 1);
			to_IDLE();
			RX_mode_enable();
			DEBUG2(("CRC Failed.\r\n"));
		}

		if((status & 0x00006000) == 0x00002000) {
			tmp = 0x20;
			Write_DW1000(0x0F, 0x01, &tmp, 1);
			DEBUG1(("We got a weird status: 0x%08X\r\n", status));
			// to_IDLE();
			// RX_mode_enable();
		}

		if((status & 0x00000080) == 0x00000080) { // transmit done
			DEBUG2(("Transmit done.\r\n"));
			tmp = 0x80;
			Write_DW1000(0x0F, 0x00, &tmp, 1);
			// clear the flag

			// Inform Host

			if(status_flag == SENT_LS_ACK) {
				DEBUG2(("LS ACK\t\tSuccessfully Sent\r\n"));
				status_flag = CONFIRM_SENT_LS_ACK;
				send_LS_DATA(mac, src);
			} else if(status_flag == CONFIRM_SENT_LS_ACK) {
				DEBUG2(("LS DATA\t\tSuccessfully Sent\r\n"));
				status_flag = SENT_LS_DATA;
				status_flag = IDLE;
				to_IDLE();
				RX_mode_enable();
				PC13_DOWN;
			}
			// currently to avoid err, cannot work as an anchor and a client at the same time
			else if(distance_flag == SENT_LS_REQ) {
				distance_flag = CONFIRM_SENT_LS_REQ;
				// Read Time Stamp
				DEBUG2(("LS Req\t\tSuccessfully Sent\r\n"));
				Read_DW1000(0x17, 0x00, (u8 *)(&Tx_stp_L), 4);
				Read_DW1000(0x17, 0x04, &Tx_stp_H, 1);
				DEBUG2(("0x%8x\r\n", Tx_stp_L));
				DEBUG2(("0x%2x\r\n", Tx_stp_H));
				to_IDLE();
				RX_mode_enable();
			} else if(distance_flag == GOT_LS_DATA) {
				// TODO
				// Successfully Sent LS RETURN
				distance_flag = IDLE;
				DEBUG2(("LS RETURN\t\tSuccessfully Sent\r\n"));
			}

		} else if (((status & 0x00004000) == 0x00004000) ||
			   ((status & 0x00002000) == 0x00002000)) { // receive done
			DEBUG2(("receive done.\r\n"));
			to_IDLE();
			// clear flag
			tmp = 0x60;
			Write_DW1000(0x0F, 0x01, &tmp, 1);

			// LS or Ethernet?
			// to me?
			// inform Host
			// send buffer to host

			raw_read(Rx_Buff, &size);
			DEBUG2(("raw_read completed.\r\n"));
			// parse_rx(Rx_Buff, size, &src, &dst, &payload, &pl_size);
			if((u8)(Rx_Buff[0]) == 0x90) { // ethernet
				for(i = 0; i < 8; i++) {
					if((u8)(Rx_Buff[1 + i]) != (u8)(broadcast_addr[i])) {
						for(i = 0; i < 8; i++) {
							if((u8)(Rx_Buff[1 + i]) != (u8)(mac[i + 2])) {
								for_me = 0;
								break;
							}
						}
						break;
					}
				}
				if(for_me == 1)
					Fifoput(Rx_Buff, size);
			} else {
				src = &(Rx_Buff[22 - 8]);
				dst = &(Rx_Buff[22 - 16]);
				payload = &(Rx_Buff[22]);
				// pl_size = (u16)(size - 22);

				// printf("\r\nGot a Frame:\r\n\
				// Frame type: %X\r\n\
				// Frame size: %d\r\n\
				// Frame Header: %02X %02X\r\n\
				// src: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n\
				// dst: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n\
				// pl_size: %d\r\n\
				// first byte of pl: %02X\r\n",
				// Rx_Buff[0]>>5, size, Rx_Buff[0], Rx_Buff[1],\
				// src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7],\
				// dst[0], dst[1], dst[2], dst[3], dst[4], dst[5], dst[6], dst[7],\
				// pl_size, payload[0]);

				DEBUG2(("Header: %02X\r\n", (u8)(Rx_Buff[0] & 0xE0)));
				for (i = 0; i < 8; i++) {
					if((u8)(dst[i]) != (u8)(broadcast_addr[i])) {
						for(i = 0; i < 8; i++) {
							if ((u8)(dst[i]) != (u8)(mac[i])) {
								for_me = 0;
								break;
							}
						}
						break;
					}
				}
				if (for_me == 1)
					// LS Frame
					if((u8)(Rx_Buff[0] & 0xE0) == 0x80) {
						DEBUG2(("A LS Frame.\r\n"));
						// GOT LS Req
						if((payload[0] == 0x00) && (status_flag == IDLE)) {
							send_LS_ACK(mac, src);
							status_flag = SENT_LS_ACK;
							DEBUG2(("\r\n===========Got LS Req===========\r\n"));
							PC13_UP;
						} else if((payload[0] == 0x01)) // GOT LS ACK
							//&&((distance_flag == CONFIRM_SENT_LS_REQ)||(distance_flag == SENT_LS_REQ))
						{
							DEBUG2(("\r\n===========Got LS ACK===========\r\n"));

							// while((u32)(status&0x00000400) == (u32)(0))
							// {
							// Delay(50);
							// read_status(&status);
							// }

							// for (i=0;i<10;i++)
							// {
							// Delay();
							// }
							// read_status(&status);
							// printf("status before read: %08X\r\n", status);
							Read_DW1000(0x15, 0x00, (u8 *)(&Rx_stp_LT[(int)(src[7] & 0x0F) - 1]), 4);
							Read_DW1000(0x15, 0x04, &Rx_stp_HT[(int)(src[7] & 0x0F) - 1], 1);
							// printf("0x%8x\r\n",Rx_stp_LT[(int)(src[7]&0x0F) - 1]);
							// printf("0x%2x\r\n",Rx_stp_HT[(int)(src[7]&0x0F) - 1]);

							// printf("0x%8x\r\n",Rx_stp_L);
							// printf("0x%2x\r\n",Rx_stp_H);
							// Read_DW1000(0x15,0x00,(u8*)(&Rx_stp_L),4);
							// Read_DW1000(0x15,0x04,&Rx_stp_H,1);
							// printf("0x%8x\r\n",Rx_stp_L);
							// printf("0x%2x\r\n",Rx_stp_H);
							// Read_DW1000(0x12,0x00,(u8 *)(&std_noise),2);
							// Read_DW1000(0x12,0x02,(u8 *)(&fp_ampl2),2);
							// Read_DW1000(0x12,0x04,(u8 *)(&fp_ampl3),2);
							// Read_DW1000(0x12,0x06,(u8 *)(&cir_mxg),2);
							// Read_DW1000(0x15,0x07,(u8 *)(&fp_ampl1),2);
							// Read_DW1000(0x10,0x02,(u8 *)(&rxpacc),2);
							to_IDLE();
							RX_mode_enable();
							PC13_UP;
						} else if(payload[0] == 0x02) { // GOT LS DATA
							#ifdef TX
							DEBUG2(("\r\n===========Got LS DATA===========\r\n"));
							distance_flag = GOT_LS_DATA;
							LS_DATA[(int)(src[7] & 0x0F) - 1] = *(u32 *)(payload + 1);
							LS_DELAY[(int)(src[7] & 0x0F) - 1] = *(u32 *)(payload + 6);
							DEBUG2(("data: %08X\r\n", LS_DATA[(int)(src[7] & 0x0F) - 1]));
							distance_measurement((int)(src[7] & 0x0F) - 1);
							// quality_measurement();
							// TODO
							// sent_LS_RETURN(mac, src);
							to_IDLE();
							RX_mode_enable();
							PC13_DOWN;
							#endif
						} else if(payload[0] == 0x03) { // GOT LS RETURN
							// TODO
							status_flag = IDLE;
						} else if(payload[0] == 0x04) { // distance forward
							handle_distance_forward(payload);
						} else {
							to_IDLE();
							RX_mode_enable();
						}
					} else {
						//Here the other data processing
						to_IDLE();
						RX_mode_enable();
					}
			}
		} else {
			to_IDLE();
			RX_mode_enable();
		}
	}
}
