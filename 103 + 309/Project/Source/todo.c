
1、过充不允许进入rtc，rtc均衡？
2、

telink  i2c研究
flash、soc、日志逻辑

//**********************************************************
todo 
1、crc 判断 spi通讯异常？？？
- 软、硬件参数确认
- 硬件参数、周期check


充电器、负载检测
分口


协议测试


/*

*/

struct RS485MSG {
	UINT8	ptr_no;          	// Word stating what state msg is in
	UINT8	csr;          		// I2C address of slave msg is intended for
	UINT16	u16RdRegStartAddr;	// read reg start addr
	UINT16	u16RdRegStartAddrActure;	//�Զ����ַ����
	UINT8	u16RdRegByteNum;    // read byte lenth
	UINT8	AckLenth;			// ack byte lenth
	UINT8	AckType;			// ack type
	UINT8	ErrorType;			// error type
	UINT8 	u16Buffer[RS485_MAX_BUFFER_SIZE];    // Array holding msg data - max that
	enum RS485_CMD_E enRs485CmdType;
};


void USART1_IRQHandler(void)
{
#ifdef _COMMOM_UPPER_SCI1
  Sci1_CommonUpper_FaultChk();
#endif

  if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
  {
    RTC_ExtComCnt++;

#ifdef _COMMOM_UPPER_SCI1
    Sci1_CommonUpper_FaultChk();
    Sci1_CommonUpper_Rx_Deal(&g_stCurrentMsgPtr_SCI1);
#endif

  }
}

void USART2_IRQHandler(void)
{
  uint32_t isr = USART2->SR;

  // ---- 1. 错误检测与清除 ----
  if (isr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE))
  {
    volatile uint32_t dump = USART2->DR;
    (void)dump;

    // 手动清除错误标志（ICR是写1清零）
    // USART2->CR = (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0);

    // gu16_CommuErrCnt_SCI2++;
    return;
  }

  // // ---- 2. 循环读取所有接收到的数据 ----
  // while (USART2->SR & USART_SR_RXNE)
  // {
  // 	uint8_t data = (uint8_t)USART2->RDR;
  // 	uart_receive_input(data);
  // }

  // #ifdef _COMMOM_UPPER_SCI2
  //   Sci2_CommonUpper_FaultChk();
  // #endif

  if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
  {
    RTC_ExtComCnt++;

#ifdef _COMMOM_UPPER_SCI2
    Sci2_CommonUpper_FaultChk();
    Sci2_CommonUpper_Rx_Deal(&g_stCurrentMsgPtr_SCI2);
#endif

  }

  // ---- 3. 可选IDLE清除 ----
  if (isr & USART_SR_IDLE)
  {
    volatile uint32_t dump = USART2->DR;
    (void)dump;
    // USART2->CR = (1 << 4); // IDLECF
  }
}

void Sci2_CommonUpper_Rx_Deal(struct RS485MSG *s)
{
	// RC1IE = 0;// 禁止EUSART2 接收中断
	// s->u16Buffer[s->ptr_no] = RCREG1;                 //读RCREG寄存器来读取接收到的8位数据
	// NVIC_DisableIRQ(USART2_IRQn);
	USART2->CR1 &= ~(1 << 5);			  // 和上面那句话二选一
	s->u16Buffer[s->ptr_no] = USART2->DR; // 从RXFIFO 中读取接收到的数据
	if ((s->ptr_no == 0) && (s->u16Buffer[0] != RS485_SLAVE_ADDR) && (s->u16Buffer[0] != RS485_BROADCAST_ADDR))
	{
		s->ptr_no = 0;
		s->u16Buffer[0] = 0;
	}
	else
	{
		if (s->ptr_no == 1)
		{
			switch (s->u16Buffer[s->ptr_no])
			{
			case RS485_CMD_READ_REGS:
				s->enRs485CmdType = RS485_CMD_READ_REGS;
				break;
			case RS485_CMD_WRITE_REG:
				s->enRs485CmdType = RS485_CMD_WRITE_REG;
				break;
			case RS485_CMD_WRITE_REGS:
				s->enRs485CmdType = RS485_CMD_WRITE_REGS;
				break;
			default:
				s->ptr_no = RS485_MAX_BUFFER_SIZE;
				s->u16Buffer[0] = 0;
				s->u16Buffer[1] = 0;
				break;
			}
		}
		else if (s->ptr_no >= 2)
		{
			switch (s->enRs485CmdType)
			{
			case RS485_CMD_READ_REGS:
			case RS485_CMD_WRITE_REG:
				if (s->ptr_no == 7)
				{ //	receive complete
					s->csr = RS485_STA_RX_COMPLETE;
					// RCSTA1bits.CREN = 0;  //禁止接收
					// RC1IE = 0;			// 禁止EUSART2 接收中断
					USART2->CR1 &= ~(1 << 2);
					USART2->CR1 &= ~(1 << 5);
				}
				break;
			case RS485_CMD_WRITE_REGS:
				if ((s->ptr_no >= 7) && (s->ptr_no == (s->u16Buffer[6] + 8)))
				{
					s->csr = RS485_STA_RX_COMPLETE;
					// disable rx TODO
					// disable rx/tx interrupt TODO
					// RCSTA1bits.CREN = 0;    //禁止接收
					// RC1IE = 0;				// 禁止EUSART2 接收中断
					USART2->CR1 &= ~(1 << 2);
					USART2->CR1 &= ~(1 << 5);
				}
				break;
			default:
				s->ptr_no = RS485_MAX_BUFFER_SIZE;
				s->u16Buffer[0] = 0;
				break;
			}
		}
		s->ptr_no++;
		if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
		{
			s->ptr_no = 0;
			s->u16Buffer[0] = 0;
		}
	}
	USART2->CR1 |= (1 << 5);
}

void Sci2_CommonUpper_Tx_Deal(struct RS485MSG *s)
{
	static int delayFlag = 0;

	if (0 == gu8_TxEnable_SCI2)
	{
		return;
	}

	if (gu16_CommuErrCnt_SCI2)
	{ // 出现错误也得把数据全部接收完，然后不回复
		s->ptr_no = 0;
		s->csr = RS485_STA_TX_COMPLETE;
		gu8_TxFinishFlag_SCI2 = 1;
		gu8_TxEnable_SCI2 = 0;
		gu16_CommuErrCnt_SCI2 = 0;
		return;
	}

	if (delayFlag)
	{
		if (g_st_SysTimeFlag.bits.b1Sys10msFlag1)
		{
			if (++delayFlag == 6)
			{
				delayFlag = 0;
			}
		}
		return;
	}

	while (!((USART2->SR) & (1 << 7)))
		; // 1<<6 也可以
	if (s->ptr_no < s->AckLenth)
	{
		USART2->DR = s->u16Buffer[s->ptr_no]; // load data
		s->ptr_no++;
		if ((s->ptr_no == 19) || (s->ptr_no == 39) || (s->ptr_no == 59))
		{
			delayFlag = 1;
		}
	}
	else
	{
		s->ptr_no = 0;
		s->csr = RS485_STA_TX_COMPLETE;
		gu8_TxFinishFlag_SCI2 = 1;
		gu8_TxEnable_SCI2 = 0;
		if (u8FlashUpdateE2PROM)
		{
			u8FlashUpdateE2PROM = 0;
			u8FlashUpdateFlag = 1;
		}
	}
}

void App_CommonUpperSCI2(struct RS485MSG *s)
{
	switch (s->csr)
	{
	// IDLE-空闲态，保持50ms后使能接收（物理层）receive set
	case RS485_STA_IDLE:
	{
		break;
	}
	// receive complete, to deal the receive data
	case RS485_STA_RX_COMPLETE:
	{
		USART2->CR1 &= ~(1 << 5); // 禁止产生中断
		CRC_verify(s);
		if (s->AckType == RS485_ACK_POS)
		{
			switch (s->enRs485CmdType)
			{
			case RS485_CMD_READ_REGS:
				Sci_Deal_ReadRegs_0x03(s);
				break;
			case RS485_CMD_WRITE_REG:
				Sci_Deal_WrReg_0x06(s);
				break;
			case RS485_CMD_WRITE_REGS:
				Sci_Deal_WrRegs_0x10(s);
				break;
			default:
				s->u16RdRegByteNum = 0;
				s->AckType = RS485_ACK_NEG;
				s->ErrorType = RS485_ERROR_NULL;
				break;
			}
		}
		s->csr = RS485_STA_RX_OK; // receive the correct data, switch to transmit wait 50ms
		break;					  // 下一轮再来
	}
	// receive ok, to transmit wait 50ms
	case RS485_STA_RX_OK:
	{
		switch (s->enRs485CmdType)
		{
		case RS485_CMD_READ_REGS:
			Sci_ACK_0x03(s);
			break;
		case RS485_CMD_WRITE_REG:
		case RS485_CMD_WRITE_REGS:
			Sci_ACK_0x06_0x10(s);
			break;
		default: // 这个defualt不用加错误操作
			break;
		}
		USART2->CR1 |= (1 << 3); // 使能发送
		gu8_TxEnable_SCI2 = 1;
	}
	// transmit complete, to switch receive wait 20ms
	case RS485_STA_TX_COMPLETE:
	{
		if (gu8_TxFinishFlag_SCI2)
		{
			s->csr = RS485_STA_IDLE;
			s->u16Buffer[0] = 0;
			s->u16Buffer[1] = 0;
			s->u16Buffer[2] = 0;
			s->u16Buffer[3] = 0;
			gu8_TxFinishFlag_SCI2 = 0;
			s->ptr_no = 0;
			USART2->CR1 |= (1 << 2); // 使能接收
			USART2->CR1 |= (1 << 5); // 使能接收中断
			gu8_TxEnable_SCI2 = 0;
		}
		break;
	}

	default:
	{
		s->csr = RS485_STA_IDLE;
		break;
	}
	}
	Sci2_CommonUpper_Tx_Deal(s);
	// Sci1_FaultChk();	//没必要在这加
}

// 给出ovt、ovh寄存器方便、简洁转换为对应保护电压、延时的代码