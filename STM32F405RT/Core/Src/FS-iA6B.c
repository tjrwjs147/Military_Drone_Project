/*
 * FS-iA6B.c
 *
 *  Created on: Sep 3, 2023
 *      Author: JANG YOUNG HYUN
 */
#include "FS-iA6B.h"

FSiA6B_iBus iBus;

// iBus CHECK SUM check
unsigned char iBus_Check_CHECKSUM(unsigned char* data, unsigned int len)
{
	unsigned short check_sum = 0xffff;

	// Sync char part(20, 40) not calculated, so temporarily calculated 20 and 40
	check_sum -= 96;

	for(int i = 2; i< len-2 ; i++)
	{
		check_sum = check_sum - data[i];
		//printf("%d, %d - %d\n", i,check_sum, data[i]);
	}

	// Initial iBus transmission data value
	// sync   /  input data                           /  junk                                             / check sum
	// 20 40  /  dc 05 dc 05 e8 03 dc 05 e8 03 e8 03  /  dc 05 dc 05 dc 05 dc 05 dc 05 dc 05 dc 05 dc 05  /  33 f3

	// CHECK SUM part test
	//printf("total check_sum : %d\n", check_sum);
	//printf("data[30] : %d\n", data[30]);
	//printf("data[31] : %d\n", data[31]);

	// return (LSB CHECK) && (MSB CHECK)
	// So if CHECK SUM value is true, then return value is 1.
	// and if CHECK SUM value is false, then return value is 0.
	return ((check_sum & 0x00ff) == data[30]) && ((check_sum>>8) == data[31]);
}

// FS-i6 data parsing
// Check the lower 12 bits for parsing
void iBus_Parsing(unsigned char* data, FSiA6B_iBus* iBus)
{
	iBus->RH = (data[2] | data[3] << 8) & 0x0fff;
	iBus->RV = (data[4] | data[5] << 8) & 0x0fff;
	iBus->LV = (data[6] | data[7] << 8) & 0x0fff;
	iBus->LH = (data[8] | data[9] << 8) & 0x0fff;
	iBus->SwA = (data[10] | data[11] << 8) & 0x0fff;
	iBus->SwC = (data[12] | data[13] << 8) & 0x0fff;

#define _USE_FS_I6
#ifdef _USE_FS_I6
	iBus->FailSafe = (data[11] >> 4);
#endif

#ifdef _USE_FS_I6X
	iBus->SwD = (data[14] | data[15] <<8) & 0x0fff;
	iBus->FailSafe = iBus->SwD == 1500;
#endif
}

// iBus failsafe check
unsigned char iBus_isActiveFailsafe(FSiA6B_iBus* iBus)
{
	// Return value is 1 when failsafe is started
	return iBus->FailSafe != 0;
}


//FSiA6B initialization
void FSiA6B_UART5_Initialization(void)
{
	LL_USART_InitTypeDef USART_InitStruct = {0};

	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* Peripheral clock enable */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART5);

	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);
	/**UART5 GPIO Configuration
	PC12   ------> UART5_TX
	PD2   ------> UART5_RX
	*/
	GPIO_InitStruct.Pin = LL_GPIO_PIN_12;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	GPIO_InitStruct.Alternate = LL_GPIO_AF_8;
	LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	GPIO_InitStruct.Alternate = LL_GPIO_AF_8;
	LL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/* UART5 interrupt Init */
	NVIC_SetPriority(UART5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(UART5_IRQn);

	/* USER CODE BEGIN UART5_Init 1 */

	/* USER CODE END UART5_Init 1 */
	USART_InitStruct.BaudRate = 115200;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_RX;
	USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
	LL_USART_Init(UART5, &USART_InitStruct);
	LL_USART_ConfigAsyncMode(UART5);
	LL_USART_Enable(UART5);
}
