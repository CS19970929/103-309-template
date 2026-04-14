#include "main.h"

void feidao_can_send(void);

struct battery_state battery = {
#if (BAT_TYPE == BAT_MASTER)
	.master_online = true,
#elif BAT_TYPE == BAT_SLAVE
	.slave_online = true,
#endif
	.state_master = s_idle,
	.state_slave = s_idle,
	.sleep = true,
};

volatile union Can_Status Can_Status_Flag;
volatile union CanTxType_Status CanTxType_Flag;
CanTxMsg TxMessage;
CanRxMsg RxMessage;

UINT16 g_u16BusOff_InitTestCnt = 0; // CAN总线关闭计时
UINT16 g_u16BusOff_RecoverCnt = 0;	// 5s计时标志位

static void feidao_put_u16_be(uint8_t *data, uint8_t offset, uint16_t value)
{
	data[offset] = (uint8_t)((value >> 8) & 0xFF);
	data[offset + 1] = (uint8_t)(value & 0xFF);
}

static void feidao_put_u32_be(uint8_t *data, uint8_t offset, uint32_t value)
{
	data[offset] = (uint8_t)((value >> 24) & 0xFF);
	data[offset + 1] = (uint8_t)((value >> 16) & 0xFF);
	data[offset + 2] = (uint8_t)((value >> 8) & 0xFF);
	data[offset + 3] = (uint8_t)(value & 0xFF);
}

static void feidao_put_i32_be(uint8_t *data, uint8_t offset, int32_t value)
{
	feidao_put_u32_be(data, offset, (uint32_t)value);
}

void CAN_TX_xintiao(void)
{
	UINT16 u16_tmp16a;
	uint8_t temp;

	// TxMessage.StdId = CANID_CHECK_0x00; // 标譨r急晔斗?
	TxMessage.StdId = 0;		// 标准标识符
	TxMessage.ExtId = 0x180000; // 扩展标识符
	// TxMessage.IDE = CAN_ID_STD; // 使用标准标识符
	TxMessage.IDE = CAN_ID_EXT;	  // 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA; // 为数据帧
	TxMessage.DLC = 8;			  // 消息的数据长度为8个字节

	TxMessage.Data[0] = (UINT8)(BAT_TYPE);
#if (BAT_TYPE == BAT_MASTER)
	battery.master_info.output = get_output_status();
#elif (BAT_TYPE == BAT_SLAVE)
	battery.slave_info.output = get_output_status();
#endif // BAT_TYPE == BAT_MASTER
	TxMessage.Data[1] = (UINT8)(get_output_status());

	// u16_tmp16a = g_stCellInfoReport.u16VCellTotle; // 电池总电流
	u16_tmp16a = g_stCellInfoReport.u16VCellMin; // 电池总电流
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	TxMessage.Data[4] = (UINT8)(g_stCellInfoReport.SocElement.u16Soc);
#if (BAT_TYPE == BAT_MASTER)
	TxMessage.Data[5] = (UINT8)(battery.state_master);
#elif (BAT_TYPE == BAT_SLAVE)
	TxMessage.Data[5] = (UINT8)(battery.state_slave);
#endif // BAT_TYPE == BAT_MASTER

#if (BAT_TYPE == BAT_MASTER)
	{
		battery.master_info.vcellmin = g_stCellInfoReport.u16VCellMin;
		battery.master_info.soc = g_stCellInfoReport.SocElement.u16Soc;
	}
	battery.master_info.fault.all = g_stCellInfoReport.unMdlFault_Third.all;
	battery.master_info.fault.bits.e2p_err = System_ERROR_UserCallback(ERROR_STATUS_EEPROM_STORE) > 0 ? 1 : 0;
	battery.master_info.fault.bits.afe_short = System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) > 0 ? 1 : 0;
	u16_tmp16a = battery.master_info.fault.all;
#elif (BAT_TYPE == BAT_SLAVE)
	{
		battery.slave_info.vcellmin = g_stCellInfoReport.u16VCellMin;
		battery.slave_info.soc = g_stCellInfoReport.SocElement.u16Soc;
	}
	battery.slave_info.fault.all = g_stCellInfoReport.unMdlFault_Third.all;
	battery.slave_info.fault.bits.e2p_err = System_ERROR_UserCallback(ERROR_STATUS_EEPROM_STORE) > 0 ? 1 : 0;
	battery.slave_info.fault.bits.afe_short = System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) > 0 ? 1 : 0;
	u16_tmp16a = battery.slave_info.fault.all;
#endif // BAT_TYPE == BAT_MASTER

	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_Tx_Data(CanTxMsg *Msg)
{
	UINT8 TXCounter = 0, TXStatus = 0;
	UINT8 u8MailBoxUsed;

	Msg->StdId += ((UINT32)CAN_ADRESS_STD_ID << 7); // 单机版地址默认为0
	u8MailBoxUsed = CAN_Transmit(CAN1, Msg);
	do
	{
		TXStatus = CAN_TransmitStatus(CAN1, u8MailBoxUsed);
		TXCounter++;
	} while ((TXStatus == CAN_TxStatus_Failed) && (TXCounter < 0xFF)); // Fail和OK不用管

	if (TXCounter >= 0xFF)
	{
		System_ERROR_UserCallback(ERROR_CAN); // 这里应该是一个Pending_Error但是Can模块不可能需要等这么久吧。
	}
}

void InitCan_GPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE); // 复用功能和GPIOB端口时钟使能

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;	  // Configure CAN pin: RX    // PD0
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;		// Configure CAN pin: TX    // PD1
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// GPIO_PinRemapConfig(GPIO_Remap2_CAN1, ENABLE);	 	//14:13位  //0：映射到PA11，PA12(默认)//1: 不用。
}

void InitCan_NVIC(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/*不用设置了，systemInit()已经做了相应操作
	#ifdef  VECT_TAB_RAM
	  NVIC_SetVectorTable(NVIC_VectTab_RAM, 0x0);
	#else
	  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);
	#endif
	*/
	/* enabling interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void InitCan_Filter(void)
{
	UINT16 u16CAN_FilterIdHigh;
	UINT16 u16CAN_FilterIdLow;
	UINT16 u16CAN_FilterMaskIdHigh;
	UINT16 u16CAN_FilterMaskIdLow;
	CAN_FilterInitTypeDef CAN_FilterInitStructure;

	// 这两句怎么来的，详细看截图——过滤器配置表
	// CAN_ID_STD之类的不需要移位，官方已经定好
	// CAN_FilterMode_IdList，列表模式FBMx = 1
	// CAN_FilterScale_16bit，过滤器组的位宽，FSCx = 0
	// 结合这两个，高位的屏蔽位也作为标识符列表，可以搞4个CANID
	// 别的情况是高位作为低位的屏蔽位。

	// stm32 can的屏蔽位模式：
	// 一个是标识符寄存器(过滤器Filter)，一个是屏蔽位寄存器(Mask)。
	// 凡是屏蔽位寄存器里为1的位所对应的标识符寄存器的位，这些位是必须匹配的
	// 也就是说，你接受到的Message里面的标识符（ID）里面对应的位必须跟标识符寄存器里对应的位相同，才能被接受。

	// 远程帧过滤器
	u16CAN_FilterIdHigh = (CANID_RX_COMMON_MSG_MASK << 5) | CAN_ID_STD | CAN_RTR_DATA;
	u16CAN_FilterIdLow = (CANID_RX_COMMON_MSG_FILTER << 5) | CAN_ID_STD | CAN_RTR_DATA;

	u16CAN_FilterMaskIdHigh = (CANID_RX_COMMON_MSG_MASK << 5) | CAN_ID_STD | CAN_RTR_DATA; // 设置成一样
	u16CAN_FilterMaskIdLow = (CANID_RX_COMMON_MSG_FILTER << 5) | CAN_ID_STD | CAN_RTR_DATA;

	CAN_FilterInitStructure.CAN_FilterNumber = 0;							// 指定过滤器为0，如果想接收多几个，范围为0——13
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;			// 指定过滤器为屏蔽模式
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_16bit;		// 过滤器位宽为16位，也即2个带屏蔽位的标准帧
	CAN_FilterInitStructure.CAN_FilterIdHigh = u16CAN_FilterIdHigh;			// 过滤器标识符的高16位值
	CAN_FilterInitStructure.CAN_FilterIdLow = u16CAN_FilterIdLow;			// 过滤器标识符的低16位值
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = u16CAN_FilterMaskIdHigh; // 过滤器屏蔽标识符的高16位值
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = u16CAN_FilterMaskIdLow;	// 过滤器屏蔽标识符的低16位值
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;	// 设定了指向过滤器的FIFO为0
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;					// 使能过滤器
	CAN_FilterInit(&CAN_FilterInitStructure);								// 按上面的参数初始化过滤器
}

void InitCan_CAN1(void)
{
	CAN_InitTypeDef CAN_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE); // CAN1 模块时钟使能
														 // APB1时钟最高36MHz

	CAN_DeInit(CAN1);					// 将外设CAN的全部寄存器重设为缺省值
	CAN_StructInit(&CAN_InitStructure); // 把CAN_InitStruct中的每一个参数按缺省值填入

	CAN_InitStructure.CAN_TTCM = DISABLE;		  // 没有使能时间触发模式
	CAN_InitStructure.CAN_ABOM = DISABLE;		  // 没有使能自动离线管理，BusOFF自动离线取消，需要手动处理
	CAN_InitStructure.CAN_AWUM = DISABLE;		  // 没有使能自动唤醒模式
	CAN_InitStructure.CAN_NART = DISABLE;		  // 没有使能非自动重传模式
	CAN_InitStructure.CAN_RFLM = DISABLE;		  // 没有使能接收FIFO锁定模式
	CAN_InitStructure.CAN_TXFP = DISABLE;		  // 没有使能发送FIFO优先级
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal; // CAN设置为正常模式
												  // CAN_InitStructure.CAN_Mode = CAN_Mode_LoopBack;

	// 关于以下的设置，sample=(1+CAN_BS1)/(1+CAN_BS1+CAN_BS2)，采样点设置在80%到87.5%之间比较好。
	// 如果can采样点选取合适，can总线就能容纳更多的can节点。因此极其重要。
	// 如果这个不行，就改为那个PDF里面的常用参考参数
	// CAN_InitStructure.CAN_SJW = CAN_SJW_1tq; // 重新同步跳跃宽度1个时间单位
	// CAN_InitStructure.CAN_BS1 = CAN_BS1_3tq; // 时间段1为3个时间单位
	// CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq; // 时间段2为2个时间单位
	// CAN_InitStructure.CAN_Prescaler = 24;	 // 时间单位长度为60
#if 1
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq; // 重新同步跳跃宽度1个时间单位
	CAN_InitStructure.CAN_BS1 = CAN_BS1_5tq; // 时间段1为3个时间单位
	CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq; // 时间段2为2个时间单位
#else
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq; // 重新同步跳跃宽度1个时间单位
	CAN_InitStructure.CAN_BS1 = CAN_BS1_2tq; // 时间段1为3个时间单位
	CAN_InitStructure.CAN_BS2 = CAN_BS2_1tq; // 时间段2为2个时间单位

#endif
	CAN_InitStructure.CAN_Prescaler = 4; // 时间单位长度为60
	CAN_Init(CAN1, &CAN_InitStructure);	 // 波特率为：72M/2/6/(1+8+3)=0.5 即500K，非PDF范例
										 // 波特率为：72M/2/12/(1+3+2)=0.5 即500K，为DPF的范例
										 // 波特率为：72M/2/24/(1+3+2)=0.25 即250K，为DPF的范例

	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE); // 使能FIFO0消息挂号中断
}

void Can_BusOFF_FaultTimeCtrl(void)
{
	if (TRUE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		g_u16BusOff_InitTestCnt++; // BUSOFF计时
	}
	if ((FALSE == (CAN1->ESR & CAN_ESR_BOFF)) && (FALSE == Can_Status_Flag.bits.b1Can_BusOFF))
	{
		g_u16BusOff_RecoverCnt++; // BUSOFF清除计时
	}
}

void Can_BusOFF_FaultChk(void)
{
	static UINT8 s_u8FlagBusOff = 0;
	if (FALSE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		if (CAN1->ESR & CAN_ESR_BOFF)
		{											  // 检测到BusOff，总线进入离线状态，找不到相关函数，自己写
			Can_Status_Flag.bits.b1Can_BusOFF = TRUE; // 找到了 --> CAN_GetFlagStatus(CAN1, CAN_FLAG_BOF)==SET
			CAN1->MCR |= CAN_MCR_INRQ;				  // 置位，从正常模式转为初始化模式(一旦当前的CAN活动(发送或接收)结束，CAN就进入初始化模式)
			s_u8FlagBusOff = 1;
			g_u16BusOff_RecoverCnt = 0; // 时序计算初始化
			g_u16BusOff_InitTestCnt = 0;
			System_ERROR_UserCallback(ERROR_CAN);
		}
	}

	if (1 == s_u8FlagBusOff)
	{ // 下一轮过来置回环模式
		s_u8FlagBusOff = 0;
		// CAN1->BTR =	(UINT32)CAN_Mode_LoopBack<<30;    	//请求环回模式，不需要静默回环模式，收到需要发ACK到总线上。
		// 真的需要改为回环模式吗，打个问号？软件INRQ位处理可在初始化模式和正常模式切换
	}
}

void Can_BusOFF_Recover(void)
{
	UINT16 u16BusOFF_InitCycleT;
	static UINT8 s_u8BusOFF_InitCnt = 0;
	if (TRUE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		if (s_u8BusOFF_InitCnt < 10)
		{											 // 快恢复计时10次
			u16BusOFF_InitCycleT = DELAYB10MS_100MS; // 快恢复计时10次100ms
		}
		else
		{
			s_u8BusOFF_InitCnt = 10;
			u16BusOFF_InitCycleT = DELAYB10MS_1S; // 1s周期
		}

		if (g_u16BusOff_InitTestCnt >= u16BusOFF_InitCycleT)
		{ // 周期初始化CAN，前10次为100ms，后面为1s
			s_u8BusOFF_InitCnt++;
			g_u16BusOff_InitTestCnt = 0;
			Can_Status_Flag.bits.b1Can_BusOFF_TestSd = 1; // 时间到尝试发送Test报文
			Can_Status_Flag.bits.b1Can_BusOFF = FALSE;
			// CAN1->BTR =	(UINT32)CAN_Mode_Normal<<30;		//请求正常模式
			CAN1->MCR &= ~CAN_MCR_INRQ; // 复位，从初始化模式转为正常模式(当CAN在接收引脚检测到连续的11个隐性位后，CAN就达到同步)
		}
	}

	if (g_u16BusOff_RecoverCnt > DELAYB10MS_500MS)
	{ // 5S内未检测到BusOFF标志，则表示与外部通信恢复正常
		g_u16BusOff_RecoverCnt = DELAYB10MS_500MS;
		s_u8BusOFF_InitCnt = 0;
	}
}

void Can_BusOFF_Monitor(void)
{
	Can_BusOFF_FaultChk();
	Can_BusOFF_FaultTimeCtrl();
	Can_BusOFF_Recover();
}

void Can_ReceiveDeal(void)
{
}

void Can_TransmitDeal(void)
{
}

void InitCan(void)
{
	Can_Status_Flag.all = 0;
	CanTxType_Flag.all = 0;
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_CAN1();	  // 目前是回环模式，要改回普通模式,Test
	InitCan_Filter(); // 这个调到后面，RX也可以了
}

bool rec_ok_other_bat_powerOn = false;
void can_send_parel_req(void)
{
}
bool check_battery_ok(uint8_t bat_num)
{
	bool res = false;

#if 0
	// if (0 == g_stCellInfoReport.unMdlFault_Third.all &&
	if (0 == g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp &&
		0 == g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp &&
		g_stCellInfoReport.SocElement.u16Soc > 0 &&
		g_stCellInfoReport.u16VCellTotle > (310 * SNum) &&
		g_stCellInfoReport.u16VCellMin > 3200)
	{
		res = true;
	}
	return res;
#endif
	// if (g_stCellInfoReport.u16VCellTotle > (310 * SNum) &&
	// 	g_stCellInfoReport.u16VCellMin > 3200)
	// {
	// 	res = true;
	// }

	if (bat_num == BAT_MASTER)
	{
		if (battery.master_info.vcellmin > 3200)
			res = true;
	}
	else if (bat_num == BAT_SLAVE)
	{
		if (battery.slave_info.vcellmin > 3200)
			res = true;
	}

	// todo 待完善
	// 定义电池状态
#if 0
	if(battery.master_info.soc == 0 && battery.slave_info.soc == 0)
	{

	}
	if(battery.master_info.vtotle < (330 * SNum) && battery.slave_info.vtotle < (330 * SNum))
	{

	}
	if(battery.master_info.fault && battery.slave_info.fault)
	{

	}
#endif
	return res;
}

void set_output_status(bool cmd)
{
	// battery.bat_output = cmd;
}

enum OUTPUT_STATUS get_output_status(void)
{
	enum OUTPUT_STATUS ret = 0;

	// todo 不行，一直读
	//  read_bms_statuse();

	if (SystemStatus.bits.b1Status_MOS_CHG && SystemStatus.bits.b1Status_MOS_DSG)
	{
		ret = CHG_DSG_ON;
	}
	else if (SystemStatus.bits.b1Status_MOS_CHG && !SystemStatus.bits.b1Status_MOS_DSG)
	{
		ret = CHG_ON_DSG_OFF;
	}
	else if (!SystemStatus.bits.b1Status_MOS_CHG && SystemStatus.bits.b1Status_MOS_DSG)
	{
		ret = CHG_OFF_DSG_ON;
	}
	else if (!SystemStatus.bits.b1Status_MOS_CHG && !SystemStatus.bits.b1Status_MOS_DSG)
	{
		ret = CHG_DSG_OFF;
	}

	return ret;
}

void set_output(bool enable)
{
	if (enable)
	{
		MCUO_AFE_CTLC = 1;
		OPEN_CHG();
		OPEN_DSG();
		battery.sleep = false;
		set_output_status(true);
	}
	else
	{
		MCUO_AFE_CTLC = 0;
		CLOSE_CHG();
		CLOSE_DSG();
		battery.sleep = true;
		set_output_status(false);
	}
}
void feidao_init(void)
{
	battery.cmd = CMD_NULL;
	set_output(true);

	read_bms_statuse();
	enum OUTPUT_STATUS out_status = get_output_status();
	// todo 单个保护？？？
#if (BAT_TYPE == BAT_MASTER)
	if (CHG_DSG_ON == out_status)
	{
		battery.state_master = s_master_output;
	}
	else
	{
		// todo
		switch (out_status)
		{
		case CHG_OFF_DSG_ON:

			break;

		default:
			break;
		}
	}
#elif (BAT_TYPE == BAT_SLAVE)
	if (CHG_DSG_ON == get_output_status())
	{
		battery.state_slave = s_slave_output;
	}
	else
	{
	}
#endif // BAT_TYPE == BAT_MASTER

	CAN_TX_xintiao();
}

void can_send(uint8_t cmd)
{
	UINT16 u16_tmp16a;
	uint8_t temp;
	// TxMessage.StdId = CANID_CHECK_0x00; // 标准标识符
	TxMessage.StdId = 1;		// 标准标识符
	TxMessage.ExtId = 0x180001; // 扩展标识符
	// TxMessage.IDE = CAN_ID_STD; // 使用标准标识符
	TxMessage.IDE = CAN_ID_EXT;	  // 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA; // 为数据帧
	TxMessage.DLC = 8;			  // 消息的数据长度为8个字节

	TxMessage.Data[0] = (UINT8)(cmd);
	TxMessage.Data[1] = (UINT8)(0);

	u16_tmp16a = g_stCellInfoReport.u16VCellTotle; // 电池总电流
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.SocElement.u16Soc; // 剩余容量10mAh
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.unMdlFault_Third.all;
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void read_bms_statuse(void)
{
	if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
	{
		SystemStatus.bits.b1Status_MOS_CHG = SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;
		SystemStatus.bits.b1Status_MOS_DSG = SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;
	}
}

void xintiao(void)
{
	static uint8_t comm_cnt = 0;
	static uint16_t comm_delay = 0;
	// if (!g_st_SysTimeFlag.bits.b1Sys10msFlag1)
	// if (!g_st_SysTimeFlag.bits.b1Sys1000msFlag1)
	// return;

	if (comm_cnt != battery.comm_cnt)
	{
		comm_cnt = battery.comm_cnt;
		comm_delay = 0;
	}
	else
	{
		if (++comm_delay >= (100 * 3))
		{
#if (BAT_TYPE == BAT_MASTER)
			battery.slave_online = false;
#elif (BAT_TYPE == BAT_SLAVE)
			battery.master_online = false;
#endif // BAT_TYPE == BAT_MASTER
		}
	}

	// sys_time.send_can_cnt++;
	CAN_TX_xintiao();
}
void feidao_logi(void)
{
	feidao_can_send();
}
#if 0
void feidao_logi(void)
{
	feidao_can_send();

	// if (battery.master_info.vcellmin < 2000 || battery.slave_info.vcellmin < 2000)
	// 	return;
// 	if (battery.master_info.output == CHG_DSG_ON && battery.slave_info.output == CHG_DSG_ON)
// 	{
// 		log_w("err");
// #if (BAT_TYPE == BAT_MASTER)
// #elif (BAT_TYPE == BAT_SLAVE)
// 		set_output(false);
// 		battery.state_slave = s_idle;
// #endif // BAT_TYPE == BAT_MASTER
// 	}

	// if (!battery.master_online || !battery.slave_online)
	// 	return;

#if (BAT_TYPE == BAT_MASTER)
	switch (battery.state_master)
	{
	case s_idle:
		break;
	case s_master_output:
		if (!check_battery_ok(BAT_MASTER) && check_battery_ok(BAT_SLAVE))
		{
			// uint16_t try_times_1ms = 0;
			battery.try_times_1ms = 0;
			/* 冗余检测
			todo 1. master close chg
				 2. can send CMD_SET_OUTPUT_B
				 3. rev slave output ok
				 5. maste close dsg
			*/
			log_w("[master] send cmd[CMD_SET_OUTPUT_B] to B,output");
			CLOSE_CHG();
			do
			{
				can_send(CMD_SET_OUTPUT_B);
				__delay_ms(1);
				// try_times_1ms++;
				battery.try_times_1ms++;
				// } while (battery.cmd != CMD_RSP_OUTPUT_OK && try_times_1ms < 3000);
			} while (battery.cmd != CMD_RSP_OUTPUT_OK && battery.try_times_1ms < 3000);
			if (battery.cmd == CMD_RSP_OUTPUT_OK)
			{
				// log_w("[master] success switch!!!, times = %d", try_times_1ms);
				CLOSE_DSG();
				battery.state_master = s_slave_output;
				set_output_status(false);
			}
			else
			{
				// log_w("[master] fail switch!!!, times = %d", try_times_1ms);
				OPEN_CHG();
			}
			battery.cmd = CMD_NULL;
		}
		break;
	case s_slave_output:
		battery.sleep = true;
		if (battery.cmd == CMD_SET_OUTPUT_A)
		{
			battery.cmd = CMD_NULL;
			log_w("[master] receive salve cmd[CMD_SET_OUTPUT_A]");
			set_output(true);
			read_bms_statuse();
			if (SystemStatus.bits.b1Status_MOS_CHG && SystemStatus.bits.b1Status_MOS_DSG)
			{
				log_w("[master] master output ok, and response");
				can_send(CMD_RSP_OUTPUT_OK);
				battery.state_master = s_master_output;
			}
			else
			{
				set_output_status(false);
				log_w("[master] master output fail, and response");
				can_send(CMD_RSP_OUTPUT_FAIL);
				// todo ??? 状态？？？
			}
		}
		if (battery.state_slave == s_idle || battery.slave_info.output == CHG_DSG_OFF)
		{
			battery.state_master = s_idle;
		}
		break;

	default:
		break;
	}
#elif (BAT_TYPE == BAT_SLAVE)
	switch (battery.state_slave)
	{
	case s_idle:
		break;
	case s_master_output:
		battery.sleep = true;
		if (battery.cmd == CMD_SET_OUTPUT_B)
		{
			battery.cmd = CMD_NULL;
			log_w("[slave] receive master cmd[CMD_SET_OUTPUT_B]");
			set_output(true);
			read_bms_statuse();
			if (SystemStatus.bits.b1Status_MOS_CHG && SystemStatus.bits.b1Status_MOS_DSG)
			{
				log_w("[slave] slave output ok, and response");
				can_send(CMD_RSP_OUTPUT_OK);
				battery.state_slave = s_slave_output;
			}
			else
			{
				set_output_status(false);
				log_w("[slave] slave output fail, and response");
				can_send(CMD_RSP_OUTPUT_FAIL);
			}
		}
		if (battery.state_master == s_idle || battery.master_info.output == CHG_DSG_OFF)
		{
			battery.state_slave = s_idle;
		}
		break;
	case s_slave_output:
		if (!check_battery_ok(BAT_SLAVE) && check_battery_ok(BAT_MASTER))
		{
			// uint16_t try_times_1ms = 0;
			battery.try_times_1ms = 0;
			/* 冗余检测
			todo 1. master close chg
				 2. can send CMD_SET_OUTPUT_B
				 3. rev slave output ok
				 5. maste close dsg
			*/
			log_w("[slave] send cmd[CMD_SET_OUTPUT_A] to master");

			CLOSE_CHG();
			do
			{
				// todo 测试从发送到应答时间，mos时间，评估是否会有问题
				can_send(CMD_SET_OUTPUT_A);
				__delay_ms(1);
				// try_times_1ms++;
				battery.try_times_1ms++;
				// } while (battery.cmd != CMD_RSP_OUTPUT_OK && try_times_1ms < 3000);
			} while (battery.cmd != CMD_RSP_OUTPUT_OK && battery.try_times_1ms < 3000);
			if (battery.cmd == CMD_RSP_OUTPUT_OK)
			{
				// log_w("[master] success switch!!!, times = %d", try_times_1ms);
				CLOSE_DSG();
				set_output_status(false);
			}
			else
			{
				// log_w("[master] fail switch!!!, times = %d", try_times_1ms);
				OPEN_CHG();
			}
			battery.cmd = CMD_NULL;

			battery.state_slave = s_master_output;
		}
		break;

	default:
		break;
	}
#endif // BAT_TYPE == BAT_MASTER
}
#endif
/*
1.开机成功通信后，master.slave 都发送心跳包，检测到互相在线，根据并机逻辑开管
2.一个电池没电怎么办,暂时不考虑
3.增加log

*/
// 这个函数不能用Switch架构来解决，因为这个都是并行任务，不是串行。
void App_Can(void)
{
	// if (0 == g_st_SysTimeFlag.bits.b1Sys100msFlag)
	if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag1)
	{
		return;
	}

	Can_BusOFF_Monitor();
	Can_ReceiveDeal();
	// Can_TransmitDeal();
	feidao_logi();
}

void CAN_Battery_SendData_feidao(uint8_t chd_index, uint8_t *data, uint8_t length)
{
	CanTxMsg tx_msg;
	uint8_t index = 2;

	/* 设置CAN ID (扩展帧) */
	tx_msg.RTR = CAN_RTR_DATA; // 为数据帧
	tx_msg.IDE = CAN_ID_EXT;
	tx_msg.ExtId = 0x14F80200 | chd_index;
	// tx_msg.ExtId = (BATTERY_CAN_ID << 24) | (BROADCAST_CAN_ID << 19) |
	//   (0x00 << 16) | (index << 8) | chd_index;

	/* 设置数据长度 */
	tx_msg.DLC = length;

	/* 设置数据 */
	for (uint8_t i = 0; i < length; i++)
	{
		tx_msg.Data[i] = data[i];
	}

	/* 发送CAN消息 */
	// CAN_Transmit(&tx_msg);
	CAN_Tx_Data(&tx_msg);
}

void feidao_send_volage_current_1000ms(void)
{
	uint8_t data[8] = {0};
	int32_t current;
	uint32_t voltage = (uint32_t)g_stCellInfoReport.u16VCellTotle * 10;
	if (g_stCellInfoReport.u16IDischg > 0)
		current = -(uint32_t)g_stCellInfoReport.u16IDischg * 100;
	else
		current = (uint32_t)g_stCellInfoReport.u16Ichg * 100;

	// 实时电压（32位，MSB first）
	feidao_put_u32_be(data, 0, voltage);
	feidao_put_i32_be(data, 4, current);
	CAN_Battery_SendData_feidao(0, &data, 8);
}

void feidao_send_cap_5000ms(void)
{
	uint8_t data[8] = {0};
	// uint32_t cap = g_stCellInfoReport.SocElement.u16CapacityNow / 100 * 1000 * (3.6 * SNum);
	uint32_t real_cap = g_stCellInfoReport.SocElement.u16CapacityNow * 10 * g_stCellInfoReport.u16VCellTotle / 100;
	uint32_t design_cap = g_stCellInfoReport.SocElement.u16CapacityFactory * 10 * (36 * SNum) / 10;
	if(real_cap >= design_cap)
		real_cap = design_cap;

	feidao_put_u32_be(data, 0, real_cap);	// 实际容量，MSB first
	feidao_put_u32_be(data, 4, design_cap); // 设计容量，MSB first
	CAN_Battery_SendData_feidao(1, &data, 8);
}

void feidao_send_soc_1000ms(void)
{
	uint8_t data[8] = {0};
	uint8_t chg_status, soc;
	int8_t temp;
	uint8_t bat_type;
	uint16_t time_chg = 100;
	uint16_t res = 0;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp)
		chg_status = 2;
	else if (g_stCellInfoReport.u16Ichg)
		chg_status = 1;
	else
		chg_status = 0;
	soc = g_stCellInfoReport.SocElement.u16Soc;
	// soc = 66;
	// if(g_stCellInfoReport.u16TempMax < 400)
	temp = (int8_t)((int16_t)g_stCellInfoReport.u16TempMax / 10 - 40);

	data[0] = chg_status;
	data[1] = soc;
	data[2] = (uint8_t)temp;
	feidao_put_u16_be(data, 3, time_chg); // 剩余充电时间，MSB first

#if (BAT_TYPE == BAT_MASTER)
	bat_type = 0x00;
#elif (BAT_TYPE == BAT_SLAVE)
	bat_type = 0x01;
#endif // BAT_TYPE == BAT_MASTER
	data[5] = bat_type;
	feidao_put_u16_be(data, 6, res); // 保留字段，MSB first
	CAN_Battery_SendData_feidao(2, &data, 8);
}

void feidao_send_soh_5000ms(void)
{
	uint8_t data[8] = {0};
	uint8_t soh = g_stCellInfoReport.SocElement.u16Soh;
	uint16_t cycles = g_stCellInfoReport.SocElement.u16Cycle_times;

	data[0] = soh;
	feidao_put_u16_be(data, 1, cycles); // 循环次数，MSB first
	CAN_Battery_SendData_feidao(3, &data, 8);
}

void feidao_send_version_5000ms(void)
{
	uint8_t data[8] = {0};
	uint8_t pro_version = 1;
	uint16_t soft_version = 1;

	data[0] = pro_version;
	data[1] = soft_version;
	CAN_Battery_SendData_feidao(4, &data, 8);
}

void feidao_send_status_5000ms(void)
{
	uint8_t data[8] = {0};
	uint8_t work_status = 0;
	uint8_t exception_status = 0;
	uint16_t cap_fac, cap_now, cap_design;
	work_status |= work_status | (SystemStatus.bits.b1Status_MOS_DSG << 0);
	work_status |= work_status | (SystemStatus.bits.b1Status_MOS_CHG << 1);
	if (g_stCellInfoReport.u16Ichg)
	{
		work_status |= work_status | (1 << 2);
		//todo 
		work_status |= work_status | (1 << 3);
	}
	if (g_stCellInfoReport.u16IDischg)
		work_status |= work_status | (1 << 4);

	// exception_status |= exception_status | (g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp << 0);
	// exception_status |= exception_status | (g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp << 1);
	// exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp | g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp) << 2);
	// exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp | g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp) << 3);
	// exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp | g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp | g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp) << 4);
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp)
		exception_status = 0x02;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp)
		exception_status = 0x03;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp)
		exception_status = 0x04;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp)
		exception_status = 0x05;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp)
		exception_status = 0x06;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp)
		exception_status = 0x07;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp)
		exception_status = 0x08;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp)
		exception_status = 0x09;
	if (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
		exception_status = 0x0C;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig)
		exception_status = 0x0D;
	cap_fac = g_stCellInfoReport.SocElement.u16CapacityFactory * 10;
	cap_now = g_stCellInfoReport.SocElement.u16CapacityNow * 10;
	cap_design = g_stCellInfoReport.SocElement.u16CapacityFactory * 10;

	data[0] = work_status;
	data[1] = exception_status;
	feidao_put_u16_be(data, 2, cap_fac);	// 完全充电容量，MSB first
	feidao_put_u16_be(data, 4, cap_now);	// 当前剩余容量，MSB first
	feidao_put_u16_be(data, 6, cap_design); // 设计容量，MSB first
	CAN_Battery_SendData_feidao(5, &data, 8);
}

void feidao_send_factory_time_5000ms(void)
{
	uint8_t data[8] = {0};

	feidao_put_u16_be(data, 0, g_stCellInfoReport.SocElement.u16CapacityFactory * 10);	
	data[5] = FD_YEAR;
	data[6] = FD_MONTH;
	data[7] = FD_DAY;
	
	CAN_Battery_SendData_feidao(8, &data, 8);
}

void feidao_can_send(void)
{
	// if (sys_time.en_1)
	// 	feidao_send_volage_current_1000ms();
	// if (sys_time.en_2)
	// 	feidao_send_cap_5000ms();
	// if (sys_time.en_3)
	// 	feidao_send_soc_1000ms();
	// if (sys_time.en_4)
	// 	feidao_send_soh_5000ms();
	// if (sys_time.en_5)
	// 	feidao_send_version_5000ms();
	// if (sys_time.en_6)
	// 	feidao_send_status_5000ms();
	// if (sys_time.en_7)
	// 	feidao_send_factory_time_5000ms();
#if 1
	static uint8_t send_state = 0;
	// xintiao();

	switch (send_state)
	{
	case 0:
		feidao_send_volage_current_1000ms();
		send_state++;
		break;
	case 1:
		feidao_send_cap_5000ms();
		send_state++;
		break;
	case 2:
		feidao_send_soc_1000ms();
		send_state++;
		break;
	case 3:
		feidao_send_soh_5000ms();
		send_state++;
		break;
	case 4:
		feidao_send_version_5000ms();
		send_state++;
		break;
	case 5:
		feidao_send_status_5000ms();
		send_state++;
		break;
	case 6:
		feidao_send_factory_time_5000ms();
		send_state = 0;
		break;
	default:
		send_state = 0;
		break;
	}
#endif
}
void USB_LP_CAN1_RX0_IRQHandler(void)
{
	sys_time.can_rcv_cnt++;
	// CanRxMsg RxMessage;
	RxMessage.StdId = 0x00;
	RxMessage.ExtId = 0x00;
	RxMessage.IDE = 0;
	RxMessage.DLC = 0;
	RxMessage.FMI = 0;
	RxMessage.Data[0] = 0x00;
	RxMessage.Data[1] = 0x00;

	CAN_Receive(CAN1, CAN_FIFO0, &RxMessage); // 接收FIFO0中的数据
	Can_Status_Flag.bits.b1Can_Received = 1;
}
