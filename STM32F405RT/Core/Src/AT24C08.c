/*
 * AT24C08.c
 *
 *  Created on: Sep 5, 2023
 *      Author: tjrwj
 */

#include "i2c.h"
#include "AT24C08.h"

void AT24C08_Page_Write(unsigned char page, unsigned char* data, unsigned char len)
{
	// decAddress : Top 2 bits + device address
	unsigned char decAddress = ((page*16)>>8)<<1 | 0xA0;
	// wordAddress : Lower 2 bits
	unsigned char wordAddress = (page*16) & 0xff;

	// Turn off write inhibit mode
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_13);

	// EEPROM internal memory write
	HAL_I2C_Mem_Write(&hi2c1, decAddress, wordAddress, I2C_MEMADD_SIZE_8BIT, &data[0], 16, 1);

	HAL_Delay(1);
	// Turn on write inhibit mode
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_13);
}

void AT24C08_Page_Read(unsigned char page, unsigned char* data, unsigned char len)
{
	// decAddress : Top 2 bits + device address
	unsigned char decAddress = ((page*16)>>8)<<1 | 0xA0;
	// wordAddress : Lower 2 bits
	unsigned char wordAddress = (page*16) & 0xff;

	// EEPROM internal memory write
	HAL_I2C_Mem_Read(&hi2c1, decAddress, wordAddress, I2C_MEMADD_SIZE_8BIT, &data[0], 16, 1);
}

void EP_PIDGain_Write(unsigned char id, float PGain, float IGain, float DGain)
{
	unsigned char buf_write[16];
	Parser parser;

	buf_write[0] = 0x45;
	buf_write[1] = 0x50;
	buf_write[2] = id;

	parser.f = PGain;
	buf_write[3] = parser.byte[0];
	buf_write[4] = parser.byte[1];
	buf_write[5] = parser.byte[2];
	buf_write[6] = parser.byte[3];

	parser.f = IGain;
	buf_write[7] = parser.byte[0];
	buf_write[8] = parser.byte[1];
	buf_write[9] = parser.byte[2];
	buf_write[10] = parser.byte[3];

	parser.f = DGain;
	buf_write[11] = parser.byte[0];
	buf_write[12] = parser.byte[1];
	buf_write[13] = parser.byte[2];
	buf_write[14] = parser.byte[3];

	unsigned char check_sum = 0xff;
	for(int i = 0; i<15;i++)
		check_sum -= buf_write[i];

	buf_write[15] = check_sum;

	switch(id)
	{
	case 0:
		AT24C08_Page_Write(0, &buf_write[0], 16);
		break;

	case 1:
		AT24C08_Page_Write(1, &buf_write[0], 16);
		break;

	case 2:
		AT24C08_Page_Write(2, &buf_write[0], 16);
		break;

	case 3:
		AT24C08_Page_Write(3, &buf_write[0], 16);
		break;

	case 4:
		AT24C08_Page_Write(4, &buf_write[0], 16);
		break;

	case 5:
		AT24C08_Page_Write(5, &buf_write[0], 16);
		break;
	}
}

unsigned char EP_PIDGain_Read(unsigned char id, float * PGain, float* IGain, float* DGain)
{
	unsigned char buf_read[16];

	Parser parser;

	switch(id)
	{
	case 0:
		AT24C08_Page_Read(0, &buf_read[0], 16);
		break;

	case 1:
		AT24C08_Page_Read(1, &buf_read[0], 16);
		break;

	case 2:
		AT24C08_Page_Read(2, &buf_read[0], 16);
		break;

	case 3:
		AT24C08_Page_Read(3, &buf_read[0], 16);
		break;

	case 4:
		AT24C08_Page_Read(4, &buf_read[0], 16);
		break;

	case 5:
		AT24C08_Page_Read(5, &buf_read[0], 16);
		break;
	}

	unsigned char check_sum = 0xff;

	for(int i = 0; i< 15; i++)
		check_sum -= buf_read[i];

	if(buf_read[15] == check_sum && buf_read[0] == 0x45 && buf_read[1] == 0x50)
	{
		parser.byte[0] = buf_read[3];
		parser.byte[1] = buf_read[4];
		parser.byte[2] = buf_read[5];
		parser.byte[3] = buf_read[6];
		*PGain = parser.f;

		parser.byte[0] = buf_read[7];
		parser.byte[1] = buf_read[8];
		parser.byte[2] = buf_read[9];
		parser.byte[3] = buf_read[10];
		*IGain = parser.f;

		parser.byte[0] = buf_read[11];
		parser.byte[1] = buf_read[12];
		parser.byte[2] = buf_read[13];
		parser.byte[3] = buf_read[14];
		*DGain = parser.f;

		return 0;
	}

	return 1;
}
