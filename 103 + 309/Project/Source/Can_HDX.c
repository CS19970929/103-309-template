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

UINT16 g_u16BusOff_InitTestCnt = 0; // CAN×ÜÏß¹Ø±Õ¼ÆÊ±
UINT16 g_u16BusOff_RecoverCnt = 0;	// 5s¼ÆÊ±±êÖ¾Î»

void CAN_TX_xintiao(void)
{
	UINT16 u16_tmp16a;
	uint8_t temp;

	// TxMessage.StdId = CANID_CHECK_0x00; // ±ê×ar¼±êÊ¶·û
	TxMessage.StdId = 0;		// ±ê×¼±êÊ¶·û
	TxMessage.ExtId = 0x180000; // À©Õ¹±êÊ¶·û
	// TxMessage.IDE = CAN_ID_STD; // Ê¹ÓÃ±ê×¼±êÊ¶·û
	TxMessage.IDE = CAN_ID_EXT;	  // Ê¹ÓÃ±ê×¼±êÊ¶·û
	TxMessage.RTR = CAN_RTR_DATA; // ÎªÊı¾İÖ¡
	TxMessage.DLC = 8;			  // ÏûÏ¢µÄÊı¾İ³¤¶ÈÎª8¸ö×Ö½Ú

	TxMessage.Data[0] = (UINT8)(BAT_TYPE);
#if (BAT_TYPE == BAT_MASTER)
	battery.master_info.output = get_output_status();
#elif (BAT_TYPE == BAT_SLAVE)
	battery.slave_info.output = get_output_status();
#endif // BAT_TYPE == BAT_MASTER
	TxMessage.Data[1] = (UINT8)(get_output_status());

	// u16_tmp16a = g_stCellInfoReport.u16VCellTotle; // µç³Ø×ÜµçÁ÷
	u16_tmp16a = g_stCellInfoReport.u16VCellMin; // µç³Ø×ÜµçÁ÷
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

	Msg->StdId += ((UINT32)CAN_ADRESS_STD_ID << 7); // µ¥»ú°æµØÖ·Ä¬ÈÏÎª0
	u8MailBoxUsed = CAN_Transmit(CAN1, Msg);
	do
	{
		TXStatus = CAN_TransmitStatus(CAN1, u8MailBoxUsed);
		TXCounter++;
	} while ((TXStatus == CAN_TxStatus_Failed) && (TXCounter < 0xFF)); // FailºÍOK²»ÓÃ¹Ü

	if (TXCounter >= 0xFF)
	{
		System_ERROR_UserCallback(ERROR_CAN); // ÕâÀïÓ¦¸ÃÊÇÒ»¸öPending_Errorµ«ÊÇCanÄ£¿é²»¿ÉÄÜĞèÒªµÈÕâÃ´¾Ã°É¡£
	}
}

void InitCan_GPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE); // ¸´ÓÃ¹¦ÄÜºÍGPIOB¶Ë¿ÚÊ±ÖÓÊ¹ÄÜ

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;	  // Configure CAN pin: RX    // PD0
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // ÉÏÀ­ÊäÈë
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;		// Configure CAN pin: TX    // PD1
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // ¸´ÓÃÍÆÍìÊä³ö
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// GPIO_PinRemapConfig(GPIO_Remap2_CAN1, ENABLE);	 	//14:13Î»  //0£ºÓ³Éäµ½PA11£¬PA12(Ä¬ÈÏ)//1: ²»ÓÃ¡£
}

void InitCan_NVIC(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/*²»ÓÃÉèÖÃÁË£¬systemInit()ÒÑ¾­×öÁËÏàÓ¦²Ù×÷
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

	// ÕâÁ½¾äÔõÃ´À´µÄ£¬ÏêÏ¸¿´½ØÍ¼¡ª¡ª¹ıÂËÆ÷ÅäÖÃ±í
	// CAN_ID_STDÖ®ÀàµÄ²»ĞèÒªÒÆÎ»£¬¹Ù·½ÒÑ¾­¶¨ºÃ
	// CAN_FilterMode_IdList£¬ÁĞ±íÄ£Ê½FBMx = 1
	// CAN_FilterScale_16bit£¬¹ıÂËÆ÷×éµÄÎ»¿í£¬FSCx = 0
	// ½áºÏÕâÁ½¸ö£¬¸ßÎ»µÄÆÁ±ÎÎ»Ò²×÷Îª±êÊ¶·ûÁĞ±í£¬¿ÉÒÔ¸ã4¸öCANID
	// ±ğµÄÇé¿öÊÇ¸ßÎ»×÷ÎªµÍÎ»µÄÆÁ±ÎÎ»¡£

	// stm32 canµÄÆÁ±ÎÎ»Ä£Ê½£º
	// Ò»¸öÊÇ±êÊ¶·û¼Ä´æÆ÷(¹ıÂËÆ÷Filter)£¬Ò»¸öÊÇÆÁ±ÎÎ»¼Ä´æÆ÷(Mask)¡£
	// ·²ÊÇÆÁ±ÎÎ»¼Ä´æÆ÷ÀïÎª1µÄÎ»Ëù¶ÔÓ¦µÄ±êÊ¶·û¼Ä´æÆ÷µÄÎ»£¬ÕâĞ©Î»ÊÇ±ØĞëÆ¥ÅäµÄ
	// Ò²¾ÍÊÇËµ£¬Äã½ÓÊÜµ½µÄMessageÀïÃæµÄ±êÊ¶·û£¨ID£©ÀïÃæ¶ÔÓ¦µÄÎ»±ØĞë¸ú±êÊ¶·û¼Ä´æÆ÷Àï¶ÔÓ¦µÄÎ»ÏàÍ¬£¬²ÅÄÜ±»½ÓÊÜ¡£

	// Ô¶³ÌÖ¡¹ıÂËÆ÷
	u16CAN_FilterIdHigh = (CANID_RX_COMMON_MSG_MASK << 5) | CAN_ID_STD | CAN_RTR_DATA;
	u16CAN_FilterIdLow = (CANID_RX_COMMON_MSG_FILTER << 5) | CAN_ID_STD | CAN_RTR_DATA;

	u16CAN_FilterMaskIdHigh = (CANID_RX_COMMON_MSG_MASK << 5) | CAN_ID_STD | CAN_RTR_DATA; // ÉèÖÃ³ÉÒ»Ñù
	u16CAN_FilterMaskIdLow = (CANID_RX_COMMON_MSG_FILTER << 5) | CAN_ID_STD | CAN_RTR_DATA;

	CAN_FilterInitStructure.CAN_FilterNumber = 0;							// Ö¸¶¨¹ıÂËÆ÷Îª0£¬Èç¹ûÏë½ÓÊÕ¶à¼¸¸ö£¬·¶Î§Îª0¡ª¡ª13
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;			// Ö¸¶¨¹ıÂËÆ÷ÎªÆÁ±ÎÄ£Ê½
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_16bit;		// ¹ıÂËÆ÷Î»¿íÎª16Î»£¬Ò²¼´2¸ö´øÆÁ±ÎÎ»µÄ±ê×¼Ö¡
	CAN_FilterInitStructure.CAN_FilterIdHigh = u16CAN_FilterIdHigh;			// ¹ıÂËÆ÷±êÊ¶·ûµÄ¸ß16Î»Öµ
	CAN_FilterInitStructure.CAN_FilterIdLow = u16CAN_FilterIdLow;			// ¹ıÂËÆ÷±êÊ¶·ûµÄµÍ16Î»Öµ
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = u16CAN_FilterMaskIdHigh; // ¹ıÂËÆ÷ÆÁ±Î±êÊ¶·ûµÄ¸ß16Î»Öµ
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = u16CAN_FilterMaskIdLow;	// ¹ıÂËÆ÷ÆÁ±Î±êÊ¶·ûµÄµÍ16Î»Öµ
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;	// Éè¶¨ÁËÖ¸Ïò¹ıÂËÆ÷µÄFIFOÎª0
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;					// Ê¹ÄÜ¹ıÂËÆ÷
	CAN_FilterInit(&CAN_FilterInitStructure);								// °´ÉÏÃæµÄ²ÎÊı³õÊ¼»¯¹ıÂËÆ÷
}

void InitCan_CAN1(void)
{
	CAN_InitTypeDef CAN_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE); // CAN1 Ä£¿éÊ±ÖÓÊ¹ÄÜ
														 // APB1Ê±ÖÓ×î¸ß36MHz

	CAN_DeInit(CAN1);					// ½«ÍâÉèCANµÄÈ«²¿¼Ä´æÆ÷ÖØÉèÎªÈ±Ê¡Öµ
	CAN_StructInit(&CAN_InitStructure); // °ÑCAN_InitStructÖĞµÄÃ¿Ò»¸ö²ÎÊı°´È±Ê¡ÖµÌîÈë

	CAN_InitStructure.CAN_TTCM = DISABLE;		  // Ã»ÓĞÊ¹ÄÜÊ±¼ä´¥·¢Ä£Ê½
	CAN_InitStructure.CAN_ABOM = DISABLE;		  // Ã»ÓĞÊ¹ÄÜ×Ô¶¯ÀëÏß¹ÜÀí£¬BusOFF×Ô¶¯ÀëÏßÈ¡Ïû£¬ĞèÒªÊÖ¶¯´¦Àí
	CAN_InitStructure.CAN_AWUM = DISABLE;		  // Ã»ÓĞÊ¹ÄÜ×Ô¶¯»½ĞÑÄ£Ê½
	CAN_InitStructure.CAN_NART = DISABLE;		  // Ã»ÓĞÊ¹ÄÜ·Ç×Ô¶¯ÖØ´«Ä£Ê½
	CAN_InitStructure.CAN_RFLM = DISABLE;		  // Ã»ÓĞÊ¹ÄÜ½ÓÊÕFIFOËø¶¨Ä£Ê½
	CAN_InitStructure.CAN_TXFP = DISABLE;		  // Ã»ÓĞÊ¹ÄÜ·¢ËÍFIFOÓÅÏÈ¼¶
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal; // CANÉèÖÃÎªÕı³£Ä£Ê½
												  // CAN_InitStructure.CAN_Mode = CAN_Mode_LoopBack;

	// ¹ØÓÚÒÔÏÂµÄÉèÖÃ£¬sample=(1+CAN_BS1)/(1+CAN_BS1+CAN_BS2)£¬²ÉÑùµãÉèÖÃÔÚ80%µ½87.5%Ö®¼ä±È½ÏºÃ¡£
	// Èç¹ûcan²ÉÑùµãÑ¡È¡ºÏÊÊ£¬can×ÜÏß¾ÍÄÜÈİÄÉ¸ü¶àµÄcan½Úµã¡£Òò´Ë¼«ÆäÖØÒª¡£
	// Èç¹ûÕâ¸ö²»ĞĞ£¬¾Í¸ÄÎªÄÇ¸öPDFÀïÃæµÄ³£ÓÃ²Î¿¼²ÎÊı
	// CAN_InitStructure.CAN_SJW = CAN_SJW_1tq; // ÖØĞÂÍ¬²½ÌøÔ¾¿í¶È1¸öÊ±¼äµ¥Î»
	// CAN_InitStructure.CAN_BS1 = CAN_BS1_3tq; // Ê±¼ä¶Î1Îª3¸öÊ±¼äµ¥Î»
	// CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq; // Ê±¼ä¶Î2Îª2¸öÊ±¼äµ¥Î»
	// CAN_InitStructure.CAN_Prescaler = 24;	 // Ê±¼äµ¥Î»³¤¶ÈÎª60
#if 1
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq; // ÖØĞÂÍ¬²½ÌøÔ¾¿í¶È1¸öÊ±¼äµ¥Î»
	CAN_InitStructure.CAN_BS1 = CAN_BS1_5tq; // Ê±¼ä¶Î1Îª3¸öÊ±¼äµ¥Î»
	CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq; // Ê±¼ä¶Î2Îª2¸öÊ±¼äµ¥Î»
#else
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq; // ÖØĞÂÍ¬²½ÌøÔ¾¿í¶È1¸öÊ±¼äµ¥Î»
	CAN_InitStructure.CAN_BS1 = CAN_BS1_2tq; // Ê±¼ä¶Î1Îª3¸öÊ±¼äµ¥Î»
	CAN_InitStructure.CAN_BS2 = CAN_BS2_1tq; // Ê±¼ä¶Î2Îª2¸öÊ±¼äµ¥Î»

#endif
	CAN_InitStructure.CAN_Prescaler = 4; // Ê±¼äµ¥Î»³¤¶ÈÎª60
	CAN_Init(CAN1, &CAN_InitStructure);	 // ²¨ÌØÂÊÎª£º72M/2/6/(1+8+3)=0.5 ¼´500K£¬·ÇPDF·¶Àı
										 // ²¨ÌØÂÊÎª£º72M/2/12/(1+3+2)=0.5 ¼´500K£¬ÎªDPFµÄ·¶Àı
										 // ²¨ÌØÂÊÎª£º72M/2/24/(1+3+2)=0.25 ¼´250K£¬ÎªDPFµÄ·¶Àı

	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE); // Ê¹ÄÜFIFO0ÏûÏ¢¹ÒºÅÖĞ¶Ï
}

void Can_BusOFF_FaultTimeCtrl(void)
{
	if (TRUE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		g_u16BusOff_InitTestCnt++; // BUSOFF¼ÆÊ±
	}
	if ((FALSE == (CAN1->ESR & CAN_ESR_BOFF)) && (FALSE == Can_Status_Flag.bits.b1Can_BusOFF))
	{
		g_u16BusOff_RecoverCnt++; // BUSOFFÇå³ı¼ÆÊ±
	}
}

void Can_BusOFF_FaultChk(void)
{
	static UINT8 s_u8FlagBusOff = 0;
	if (FALSE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		if (CAN1->ESR & CAN_ESR_BOFF)
		{											  // ¼ì²âµ½BusOff£¬×ÜÏß½øÈëÀëÏß×´Ì¬£¬ÕÒ²»µ½Ïà¹Øº¯Êı£¬×Ô¼ºĞ´
			Can_Status_Flag.bits.b1Can_BusOFF = TRUE; // ÕÒµ½ÁË --> CAN_GetFlagStatus(CAN1, CAN_FLAG_BOF)==SET
			CAN1->MCR |= CAN_MCR_INRQ;				  // ÖÃÎ»£¬´ÓÕı³£Ä£Ê½×ªÎª³õÊ¼»¯Ä£Ê½(Ò»µ©µ±Ç°µÄCAN»î¶¯(·¢ËÍ»ò½ÓÊÕ)½áÊø£¬CAN¾Í½øÈë³õÊ¼»¯Ä£Ê½)
			s_u8FlagBusOff = 1;
			g_u16BusOff_RecoverCnt = 0; // Ê±Ğò¼ÆËã³õÊ¼»¯
			g_u16BusOff_InitTestCnt = 0;
			System_ERROR_UserCallback(ERROR_CAN);
		}
	}

	if (1 == s_u8FlagBusOff)
	{ // ÏÂÒ»ÂÖ¹ıÀ´ÖÃ»Ø»·Ä£Ê½
		s_u8FlagBusOff = 0;
		// CAN1->BTR =	(UINT32)CAN_Mode_LoopBack<<30;    	//ÇëÇó»·»ØÄ£Ê½£¬²»ĞèÒª¾²Ä¬»Ø»·Ä£Ê½£¬ÊÕµ½ĞèÒª·¢ACKµ½×ÜÏßÉÏ¡£
		// ÕæµÄĞèÒª¸ÄÎª»Ø»·Ä£Ê½Âğ£¬´ò¸öÎÊºÅ£¿Èí¼şINRQÎ»´¦Àí¿ÉÔÚ³õÊ¼»¯Ä£Ê½ºÍÕı³£Ä£Ê½ÇĞ»»
	}
}

void Can_BusOFF_Recover(void)
{
	UINT16 u16BusOFF_InitCycleT;
	static UINT8 s_u8BusOFF_InitCnt = 0;
	if (TRUE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		if (s_u8BusOFF_InitCnt < 10)
		{											 // ¿ì»Ö¸´¼ÆÊ±10´Î
			u16BusOFF_InitCycleT = DELAYB10MS_100MS; // ¿ì»Ö¸´¼ÆÊ±10´Î100ms
		}
		else
		{
			s_u8BusOFF_InitCnt = 10;
			u16BusOFF_InitCycleT = DELAYB10MS_1S; // 1sÖÜÆÚ
		}

		if (g_u16BusOff_InitTestCnt >= u16BusOFF_InitCycleT)
		{ // ÖÜÆÚ³õÊ¼»¯CAN£¬Ç°10´ÎÎª100ms£¬ºóÃæÎª1s
			s_u8BusOFF_InitCnt++;
			g_u16BusOff_InitTestCnt = 0;
			Can_Status_Flag.bits.b1Can_BusOFF_TestSd = 1; // Ê±¼äµ½³¢ÊÔ·¢ËÍTest±¨ÎÄ
			Can_Status_Flag.bits.b1Can_BusOFF = FALSE;
			// CAN1->BTR =	(UINT32)CAN_Mode_Normal<<30;		//ÇëÇóÕı³£Ä£Ê½
			CAN1->MCR &= ~CAN_MCR_INRQ; // ¸´Î»£¬´Ó³õÊ¼»¯Ä£Ê½×ªÎªÕı³£Ä£Ê½(µ±CANÔÚ½ÓÊÕÒı½Å¼ì²âµ½Á¬ĞøµÄ11¸öÒşĞÔÎ»ºó£¬CAN¾Í´ïµ½Í¬²½)
		}
	}

	if (g_u16BusOff_RecoverCnt > DELAYB10MS_500MS)
	{ // 5SÄÚÎ´¼ì²âµ½BusOFF±êÖ¾£¬Ôò±íÊ¾ÓëÍâ²¿Í¨ĞÅ»Ö¸´Õı³£
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
	InitCan_CAN1();	  // Ä¿Ç°ÊÇ»Ø»·Ä£Ê½£¬Òª¸Ä»ØÆÕÍ¨Ä£Ê½,Test
	InitCan_Filter(); // Õâ¸öµ÷µ½ºóÃæ£¬RXÒ²¿ÉÒÔÁË
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

	// todo ´ıÍêÉÆ
	// ¶¨Òåµç³Ø×´Ì¬
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

	// todo ²»ĞĞ£¬Ò»Ö±¶Á
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
	// todo µ¥¸ö±£»¤£¿£¿£¿
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
	// TxMessage.StdId = CANID_CHECK_0x00; // ±ê×¼±êÊ¶·û
	TxMessage.StdId = 1;		// ±ê×¼±êÊ¶·û
	TxMessage.ExtId = 0x180001; // À©Õ¹±êÊ¶·û
	// TxMessage.IDE = CAN_ID_STD; // Ê¹ÓÃ±ê×¼±êÊ¶·û
	TxMessage.IDE = CAN_ID_EXT;	  // Ê¹ÓÃ±ê×¼±êÊ¶·û
	TxMessage.RTR = CAN_RTR_DATA; // ÎªÊı¾İÖ¡
	TxMessage.DLC = 8;			  // ÏûÏ¢µÄÊı¾İ³¤¶ÈÎª8¸ö×Ö½Ú

	TxMessage.Data[0] = (UINT8)(cmd);
	TxMessage.Data[1] = (UINT8)(0);

	u16_tmp16a = g_stCellInfoReport.u16VCellTotle; // µç³Ø×ÜµçÁ÷
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.SocElement.u16Soc; // Ê£ÓàÈİÁ¿10mAh
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
			/* ÈßÓà¼ì²â
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
				// todo ??? ×´Ì¬£¿£¿£¿
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
			/* ÈßÓà¼ì²â
			todo 1. master close chg
				 2. can send CMD_SET_OUTPUT_B
				 3. rev slave output ok
				 5. maste close dsg
			*/
			log_w("[slave] send cmd[CMD_SET_OUTPUT_A] to master");

			CLOSE_CHG();
			do
			{
				// todo ²âÊÔ´Ó·¢ËÍµ½Ó¦´ğÊ±¼ä£¬mosÊ±¼ä£¬ÆÀ¹ÀÊÇ·ñ»áÓĞÎÊÌâ
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
1.¿ª»ú³É¹¦Í¨ĞÅºó£¬master.slave ¶¼·¢ËÍĞÄÌø°ü£¬¼ì²âµ½»¥ÏàÔÚÏß£¬¸ù¾İ²¢»úÂß¼­¿ª¹Ü
2.Ò»¸öµç³ØÃ»µçÔõÃ´°ì,ÔİÊ±²»¿¼ÂÇ
3.Ôö¼Ólog

*/
// Õâ¸öº¯Êı²»ÄÜÓÃSwitch¼Ü¹¹À´½â¾ö£¬ÒòÎªÕâ¸ö¶¼ÊÇ²¢ĞĞÈÎÎñ£¬²»ÊÇ´®ĞĞ¡£
void App_Can(void)
{
	if (0 == g_st_SysTimeFlag.bits.b1Sys100msFlag)
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

	/* ÉèÖÃCAN ID (À©Õ¹Ö¡) */
	tx_msg.RTR = CAN_RTR_DATA; // ÎªÊı¾İÖ¡
	tx_msg.IDE = CAN_ID_EXT;
	tx_msg.ExtId = 0x141F0200 | chd_index;
	// tx_msg.ExtId = (BATTERY_CAN_ID << 24) | (BROADCAST_CAN_ID << 19) |
	//   (0x00 << 16) | (index << 8) | chd_index;

	/* ÉèÖÃÊı¾İ³¤¶È */
	tx_msg.DLC = length;

	/* ÉèÖÃÊı¾İ */
	for (uint8_t i = 0; i < length; i++)
	{
		tx_msg.Data[i] = data[i];
	}

	/* ·¢ËÍCANÏûÏ¢ */
	// CAN_Transmit(&tx_msg);
	CAN_Tx_Data(&tx_msg);
}

void feidao_send_volage_current_1000ms(void)
{
	uint8_t data[8];
	int32_t current;
	uint32_t voltage = (uint32_t)g_stCellInfoReport.u16VCellTotle * 10;
	if (g_stCellInfoReport.u16IDischg > 0)
		current = -(uint32_t)g_stCellInfoReport.u16IDischg * 100;
	else
		current = (uint32_t)g_stCellInfoReport.u16Ichg * 100;

	// ÊµÊ±µçÑ¹£¨32Î»£¬LSB first£©
	// data[0] = (voltage >> 0) & 0xFF;
	// data[1] = (voltage >> 8) & 0xFF;
	// data[2] = (voltage >> 16) & 0xFF;
	// data[3] = (voltage >> 24) & 0xFF;
	memcpy(&data[0], &voltage, 4); // µçÑ¹£¬×Ô¶¯LSB first
	memcpy(&data[4], &current, 4); // ÎÂ¶È£¬×Ô¶¯LSB first
	CAN_Battery_SendData_feidao(0, &data, 8);
}

void feidao_send_cap_5000ms(void)
{
	uint8_t data[8];
	// uint32_t cap = g_stCellInfoReport.SocElement.u16CapacityNow / 100 * 1000 * (3.6 * SNum);
	uint32_t real_cap = g_stCellInfoReport.SocElement.u16CapacityNow * 100 * (3.6 * SNum);
	uint32_t design_cap = g_stCellInfoReport.SocElement.u16CapacityFactory * 100 * (3.6 * SNum);

	memcpy(&data[0], &real_cap, 4);	  // µçÑ¹£¬×Ô¶¯LSB first
	memcpy(&data[4], &design_cap, 4); // ÎÂ¶È£¬×Ô¶¯LSB first
	CAN_Battery_SendData_feidao(1, &data, 8);
}

void feidao_send_soc_1000ms(void)
{
	uint8_t data[8];
	uint8_t chg_status, soc;
	int8_t temp;
	uint16_t bat_type;
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

	memcpy(&data[0], &chg_status, 1); // µçÑ¹£¬×Ô¶¯LSB first
	memcpy(&data[1], &soc, 1);		  // ÎÂ¶È£¬×Ô¶¯LSB first
	memcpy(&data[2], &temp, 1);		  // ÎÂ¶È£¬×Ô¶¯LSB first
	memcpy(&data[3], &time_chg, 2);	  // ÎÂ¶È£¬×Ô¶¯LSB first

#if (BAT_TYPE == BAT_MASTER)
	bat_type = 0x01;
#elif (BAT_TYPE == BAT_SLAVE)
	bat_type = 0x02;
#endif								// BAT_TYPE == BAT_MASTER
	memcpy(&data[5], &bat_type, 1); // ÎÂ¶È£¬×Ô¶¯LSB first
	memcpy(&data[6], &res, 2); // ÎÂ¶È£¬×Ô¶¯LSB first
	CAN_Battery_SendData_feidao(2, &data, 8);
}

void feidao_send_soh_5000ms(void)
{
	uint8_t data[8];
	uint8_t soh = g_stCellInfoReport.SocElement.u16Soh;
	uint16_t cycles = g_stCellInfoReport.SocElement.u16Cycle_times;

	memcpy(&data[0], &soh, 1);	  // µçÑ¹£¬×Ô¶¯LSB first
	memcpy(&data[1], &cycles, 2); // ÎÂ¶È£¬×Ô¶¯LSB first
	CAN_Battery_SendData_feidao(3, &data, 8);
}

void feidao_send_version_5000ms(void)
{
	uint8_t data[8];
	uint8_t pro_version = 1;
	uint16_t soft_version = 1;

	memcpy(&data[0], &pro_version, 1);	// µçÑ¹£¬×Ô¶¯LSB first
	memcpy(&data[1], &soft_version, 1); // ÎÂ¶È£¬×Ô¶¯LSB first
	CAN_Battery_SendData_feidao(4, &data, 8);
}

void feidao_send_status_5000ms(void)
{
	uint8_t data[8];
	uint8_t work_status = 0;
	uint8_t exception_status;
	uint16_t cap_fac, cap_now, cap_design;
	work_status |= work_status | (SystemStatus.bits.b1Status_MOS_DSG << 0);
	work_status |= work_status | (SystemStatus.bits.b1Status_MOS_CHG << 1);
	if (g_stCellInfoReport.u16Ichg)
	{
		work_status |= work_status | (1 << 2);
		work_status |= work_status | (1 << 3);
	}
	if (g_stCellInfoReport.u16IDischg)
		work_status |= work_status | (1 << 4);

	exception_status |= exception_status | (g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp << 0);
	exception_status |= exception_status | (g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp << 1);
	exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp | g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp) << 2);
	exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp | g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp) << 3);
	exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp | g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp | g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp) << 4);
	cap_fac = g_stCellInfoReport.SocElement.u16CapacityFactory * 10;
	cap_now = g_stCellInfoReport.SocElement.u16CapacityNow * 10;
	cap_design = g_stCellInfoReport.SocElement.u16CapacityFactory * 10;

	memcpy(&data[0], &work_status, 1);		// µçÑ¹£¬×Ô¶¯LSB first
	memcpy(&data[1], &exception_status, 1); // ÎÂ¶È£¬×Ô¶¯LSB first
	memcpy(&data[2], &cap_fac, 2);			// ÎÂ¶È£¬×Ô¶¯LSB first
	memcpy(&data[4], &cap_now, 2);			// ÎÂ¶È£¬×Ô¶¯LSB first
	memcpy(&data[6], &cap_design, 2);		// ÎÂ¶È£¬×Ô¶¯LSB first
	CAN_Battery_SendData_feidao(5, &data, 8);
}
void feidao_can_send(void)
{
	// feidao_send_soc_1000ms();
#if 1
	// if (!g_st_SysTimeFlag.bits.b1Sys1000msFlag1)
	// if (!g_st_SysTimeFlag.bits.b1Sys10msFlag1)
	// 	return;
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

	CAN_Receive(CAN1, CAN_FIFO0, &RxMessage); // ½ÓÊÕFIFO0ÖĞµÄÊı¾İ
	Can_Status_Flag.bits.b1Can_Received = 1;
}
