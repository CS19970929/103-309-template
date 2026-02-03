#include "main.h"

#define TM7705_RCC_CS 		RCC_APB2Periph_GPIOA
#define TM7705_PORT_CS		GPIO_CS_SPI
#define TM7705_PIN_CS		PIN_CS_SPI

#define TM7705_CS_1()		TM7705_PORT_CS->BSRR = TM7705_PIN_CS
#define TM7705_CS_0()		TM7705_PORT_CS->BSRR = TM7705_PIN_CS	

void TM7705_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* 占用SPI总线， 用于总线共享 */

		#ifdef SOFT_SPI		/* 软件SPI */
			bsp_SetSpiSck(1);
			TM7705_CS_0();
		#endif

		
	}
	else
	{
		TM7705_CS_1();

		bsp_SpiBusExit();	/* 释放SPI总线， 用于总线共享 */
	}
}

static void TM7705_WriteByte(uint8_t _data)
{
	// TM7705_SetCS(0);
	bsp_spiWrite1(_data);
	// TM7705_SetCS(1;
}

static void TM7705_Write3Byte(uint32_t _data)
{
	TM7705_SetCS(0);
	bsp_spiWrite1((_data >> 16) & 0xFF);
	bsp_spiWrite1((_data >> 8) & 0xFF);
	bsp_spiWrite1(_data);
	TM7705_SetCS(1);
}

static uint8_t TM7705_ReadByte(void)
{
	uint8_t read;

	TM7705_SetCS(0);
	read = bsp_spiRead1();
	TM7705_SetCS(1);

	return read;
}

static uint16_t TM7705_Read2Byte(void)
{
	uint16_t read;

	// TM7705_SetCS(0);
	read = bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	// TM7705_SetCS(1);

	return read;
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_Read3Byte
*	功能说明: 读3字节数据
*	形    参: 无
*	返 回 值: 读取到的数据（24bit) 高8位固定为0.
*********************************************************************************************************
*/
static uint32_t TM7705_Read3Byte(void)
{
	uint32_t read;

	TM7705_SetCS(0);
	read = bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	TM7705_SetCS(1);
	return read;
}

bool sh3673520_spi_read_be_u16_test(sh3673520_spi_t *s, uint8_t reg_hi, uint16_t *out)
{
    uint16_t read;
    uint8_t crc8;
    uint16_t read_len = 2;
    uint8_t buf[2] = {0};
    if (!out) return false;

    /* IMPORTANT: must read from high-byte address and read 2 bytes continuously */
    // if (!sh3673520_spi_read(s, reg_hi, buf, 2)) return false;
    TM7705_SetCS(0);

    TM7705_WriteByte(0x02);
    TM7705_WriteByte(reg_hi);
    TM7705_WriteByte(read_len);

    TM7705_ReadByte();
    read = TM7705_Read2Byte();
    crc8 = TM7705_ReadByte();

    TM7705_SetCS(1);

    *out = read;

    *out = ((uint16_t)buf[0] << 8) | buf[1];
    return true;
}
