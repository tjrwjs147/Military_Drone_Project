/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "BNO080.h"
#include "Quaternion.h"
#include "ICM20602.h"
#include "LPS22HH.h"
#include "FS-iA6B.h"
#include "AT24C08.h"
#include "M8N.h"
#include <string.h>
#include "PID control.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// Send usart6 data to PC
int _write(int file, char* p, int len)
{
	for(int i = 0; i< len ; i++)
	{
		while(!LL_USART_IsActiveFlag_TXE(USART6));
		LL_USART_TransmitData8(USART6, *(p+i));
		//HAL_Delay(1);
	}
	return len;
}
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern	uint8_t uart6_rx_flag;
extern	uint8_t uart6_rx_data;

//GPS_UBX protocol
extern uint8_t m8n_rx_buf[36];
extern uint8_t m8n_rx_cplt_flag;

extern	uint8_t ibus_rx_buf[32];
extern	uint8_t ibus_rx_cplt_flag;

//telemetry
extern uint8_t uart1_rx_data;

uint8_t telemetry_tx_buf[40];
uint8_t telemetry_rx_buf[20];
uint8_t telemetry_rx_cplt_flag;

extern uint8_t tim7_1ms_flag;
extern uint8_t tim7_20ms_flag;
extern uint8_t tim7_100ms_flag;
extern uint8_t tim7_1000ms_flag;

float batVolt;

unsigned char failsafe_flag = 0;
unsigned char low_bat_flag = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
int Is_iBus_Throttle_Min(void);
void ESC_Calibration(void);
int Is_iBus_Received(void);
void BNO080_Calibration(void);

void Encode_Msg_AHRS(unsigned char* telemetry_tx_buf);
void Encode_Msg_GPS(unsigned char* telemetry_tx_buf);
void Encode_Msg_PID_Gain(unsigned char* telemetry_tx_buf, unsigned char id, float p, float i, float d);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define X 0.90f
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	//BNO080 sensor value
	float q[4];
	float quatRadianAccuracy;
	unsigned short adcVal;

	short gyro_x_offset = 5;
	short gyro_y_offset = 15;
	short gyro_z_offset = -5;

	unsigned char motor_arming_flag = 0;
	unsigned short iBus_SwA_Prev = 0;

	unsigned char iBus_rx_cnt = 0;
	unsigned short ccr1;
	unsigned short ccr2;
	unsigned short ccr3;
	unsigned short ccr4;

	float yaw_heading_reference;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM3_Init();
  MX_USART6_UART_Init();
  MX_SPI2_Init();
  MX_SPI1_Init();
  MX_SPI3_Init();
  MX_UART5_Init();
  MX_TIM5_Init();
  MX_UART4_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */

  //Peripheral check
  //TIM3 enable
  LL_TIM_EnableCounter(TIM3);

  // USART interrupt setting
  LL_USART_EnableIT_RXNE(USART6);
  LL_USART_EnableIT_RXNE(UART5);

  // USART GPS MODULE interrupt(KSY)
  LL_USART_EnableIT_RXNE(UART4);

  //telemetry
  HAL_UART_Receive_IT(&huart1, &uart1_rx_data, 1);
  //TIM5 enable
  LL_TIM_EnableCounter(TIM5);
  LL_TIM_CC_EnableChannel(TIM5, LL_TIM_CHANNEL_CH1);
  LL_TIM_CC_EnableChannel(TIM5, LL_TIM_CHANNEL_CH2);
  LL_TIM_CC_EnableChannel(TIM5, LL_TIM_CHANNEL_CH3);
  LL_TIM_CC_EnableChannel(TIM5, LL_TIM_CHANNEL_CH4);

  // Battery ADC active
  HAL_ADC_Start_DMA(&hadc1, &adcVal, 1);

  // Telemetry 20ms timer enable
  LL_TIM_EnableCounter(TIM7);
  LL_TIM_EnableIT_UPDATE(TIM7);

  TIM3->PSC = 1000;
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  HAL_Delay(60);
  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  HAL_Delay(60);
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  HAL_Delay(60);
  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  HAL_Delay(60);
  printf("Checking sensor connection...\n");

  //Sensor check
  //BNO080 initialization and check
  if(BNO080_Initialization() != 0)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 2000;
	  HAL_Delay(100);
	  TIM3->PSC = 1500;
	  HAL_Delay(100);
	  TIM3->PSC = 1000;
	  HAL_Delay(100);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  printf("\nBNO080 failed. Program shutting down...\n");
	  while(1)
	  {
		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_0);
		  HAL_Delay(200);
		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_0);
		  HAL_Delay(200);
	  }
  }
  //Rotation vector 400 count per 1 second
  //BNO080_enableRotationVector(2500);
  BNO080_enableGameRotationVector(2500);

  //ICM20602 initialization and check
  if(ICM20602_Initialization() != 0)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 2000;
	  HAL_Delay(100);
	  TIM3->PSC = 1500;
	  HAL_Delay(100);
	  TIM3->PSC = 1000;
	  HAL_Delay(100);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  printf("\nICM-20602 failed. Program shutting down...\n");
	  while(1)
	  {
		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_1);
		  HAL_Delay(200);
		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_1);
		  HAL_Delay(200);
	  }
  }

  //LPS22HH initialization and check
  if(LPS22HH_Initialization() != 0)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 2000;
	  HAL_Delay(100);
	  TIM3->PSC = 1500;
	  HAL_Delay(100);
	  TIM3->PSC = 1000;
	  HAL_Delay(100);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  printf("\nLPS22HH failed. Program shutting down...\n");
	  while(1)
	  {
		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);
		  HAL_Delay(200);
		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);
		  HAL_Delay(200);
	  }
  }

  printf("All sensors OK!\n\n");

  //Called GPS_M8N_setting_initialization from M8N.h file
  M8N_Initialization();

  // ICM20602 offset x voltage remove
  ICM20602_Writebyte(0x13, (gyro_x_offset*-2)>>8);
  ICM20602_Writebyte(0x14, (gyro_x_offset*-2));

  // ICM20602 offset y Voltage remove
  ICM20602_Writebyte(0x15, (gyro_y_offset*-2)>>8);
  ICM20602_Writebyte(0x16, (gyro_y_offset*-2));

  // ICM20602 offset z Voltage remove
  ICM20602_Writebyte(0x17, (gyro_z_offset*-2)>>8);
  ICM20602_Writebyte(0x18, (gyro_z_offset*-2));

  printf("Loading PID Gain...\n");

  if(EP_PIDGain_Read(0, &roll.in.kp, &roll.in.ki, &roll.in.kd) != 0 ||
		  EP_PIDGain_Read(1, &roll.out.kp, &roll.out.ki, &roll.out.kd) != 0 ||
		  EP_PIDGain_Read(2, &pitch.in.kp, &pitch.in.ki, &pitch.in.kd) != 0 ||
		  EP_PIDGain_Read(3, &pitch.out.kp, &pitch.out.ki, &pitch.out.kd) != 0 ||
		  EP_PIDGain_Read(4, &yaw_heading.kp, &yaw_heading.ki, &yaw_heading.kd) != 0 ||
		  EP_PIDGain_Read(5, &yaw_rate.kp, &yaw_rate.ki, &yaw_rate.kd) != 0)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 2000;
	  HAL_Delay(100);
	  TIM3->PSC = 1500;
	  HAL_Delay(100);
	  TIM3->PSC = 1000;
	  HAL_Delay(100);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  HAL_Delay(500);
	  printf("\nCouldn't load PID Gain.\n");
  }
  else
  {
	  // Read gain
	  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 0, roll.in.kp, roll.in.ki, roll.in.kd);
	  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 1, roll.out.kp, roll.out.ki, roll.out.kd);
	  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 2, pitch.in.kp, pitch.in.ki, pitch.in.kd);
	  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 3, pitch.out.kp, pitch.out.ki, pitch.out.kd);
	  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 4, yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
	  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 5, yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
	  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  printf("\nAll GAINS OK!\n\n");
  }

  // iBus input check
  while(Is_iBus_Received() == 0)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 3000;
	  HAL_Delay(200);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  HAL_Delay(200);
  }

  // ESC calibration Mode
  if(iBus.SwC == 2000)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 1500;
	  HAL_Delay(200);
	  TIM3->PSC = 2000;
	  HAL_Delay(200);

	  TIM3->PSC = 1500;
	  HAL_Delay(200);
	  TIM3->PSC = 2000;
	  HAL_Delay(200);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  // ESC calibration
	  ESC_Calibration();

	  // FS-i6 SwC check
	  while(iBus.SwC != 1000)
	  {
		  Is_iBus_Received();

		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

		  TIM3->PSC = 1500;
		  HAL_Delay(200);
		  TIM3->PSC = 2000;
		  HAL_Delay(200);

		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  }
  }
  // BNO080 calibration Mode
  else if(iBus.SwC == 1500)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 1500;
	  HAL_Delay(200);
	  TIM3->PSC = 2000;
	  HAL_Delay(200);

	  TIM3->PSC = 1500;
	  HAL_Delay(200);
	  TIM3->PSC = 2000;
	  HAL_Delay(200);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  // BNO080 calibration
	  BNO080_Calibration();

	  // FS-i6 SwC check
	  while(iBus.SwC != 1000)
	  {
		  Is_iBus_Received();

		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

		  TIM3->PSC = 1500;
		  HAL_Delay(200);
		  TIM3->PSC = 2000;
		  HAL_Delay(200);

		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  }
  }
  // Motor safety setting
  while(Is_iBus_Throttle_Min() == 0 || iBus.SwA == 2000)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 1000;
	  HAL_Delay(70);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  HAL_Delay(70);
  }

  //TIM3 enable
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

  TIM3->PSC = 2000;
  HAL_Delay(100);
  TIM3->PSC = 1500;
  HAL_Delay(100);
  TIM3->PSC = 1000;
  HAL_Delay(100);

  //TIM3 disable
  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

  printf("Start\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if(tim7_1ms_flag ==1)
	  {
		  tim7_1ms_flag = 0;

		  Double_Roll_Pitch_PID_Calculation(&pitch, (iBus.RV - 1500) * 0.1f, BNO080_Pitch, ICM20602.gyro_x);
		  Double_Roll_Pitch_PID_Calculation(&roll, (iBus.RH - 1500) * 0.1f, BNO080_Roll, ICM20602.gyro_y);

		  if(iBus.LV < 1030 || motor_arming_flag == 0)
		  {
			  Reset_All_PID_Integrator();
		  }

		  if(iBus.LH < 1485 || iBus.LH > 1515)
		  {
			  yaw_heading_reference = BNO080_Yaw;
			  Single_Yaw_Rate_PID_Calculation(&yaw_rate, (iBus.LH - 1500), ICM20602.gyro_z);

			  ccr1 = 10500 + 500 + (iBus.LV - 1000) * 10 - pitch.in.pid_result + roll.in.pid_result - yaw_rate.pid_result;
			  ccr2 = 10500 + 500 + (iBus.LV - 1000) * 10 + pitch.in.pid_result + roll.in.pid_result + yaw_rate.pid_result;
			  ccr3 = 10500 + 500 + (iBus.LV - 1000) * 10 + pitch.in.pid_result - roll.in.pid_result - yaw_rate.pid_result;
			  ccr4 = 10500 + 500 + (iBus.LV - 1000) * 10 - pitch.in.pid_result - roll.in.pid_result + yaw_rate.pid_result;
		  }
		  else
		  {
			  Single_Yaw_Heading_PID_Calculation(&yaw_heading, yaw_heading_reference, BNO080_Yaw, ICM20602.gyro_z);

			  ccr1 = 10500 + 500 + (iBus.LV - 1000) * 10 - pitch.in.pid_result + roll.in.pid_result - yaw_heading.pid_result;
			  ccr2 = 10500 + 500 + (iBus.LV - 1000) * 10 + pitch.in.pid_result + roll.in.pid_result + yaw_heading.pid_result;
			  ccr3 = 10500 + 500 + (iBus.LV - 1000) * 10 + pitch.in.pid_result - roll.in.pid_result - yaw_heading.pid_result;
			  ccr4 = 10500 + 500 + (iBus.LV - 1000) * 10 - pitch.in.pid_result - roll.in.pid_result + yaw_heading.pid_result;
		  }

		  //printf("%f\t%f\n",BNO080_Pitch, ICM20602.gyro_x);
		  //printf("%f\t%f\n",BNO080_Roll, ICM20602.gyro_y);
		  //printf("%f\t%f\n",BNO080_Yaw, ICM20602.gyro_z);
	  }

	  if(iBus.SwA == 2000 && iBus_SwA_Prev != 2000)
	  {
		  if(iBus.LV < 1020)
		  {
			  motor_arming_flag = 1;

			  yaw_heading_reference = BNO080_Yaw;
		  }
		  else
		  {
			  while(Is_iBus_Throttle_Min() == 0 || iBus.SwA == 2000)
			  {
				  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

				  TIM3->PSC = 1000;
				  HAL_Delay(70);

				  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
				  HAL_Delay(70);
			  }
		  }
	  }

	  iBus_SwA_Prev = iBus.SwA;

	  if(iBus.SwA != 2000)
	  {
		  motor_arming_flag = 0;
	  }

	  if(motor_arming_flag == 1)
	  {
		  if(failsafe_flag == 0)
		  {
			  if(iBus.LV > 1030)
			  {
				  TIM5->CCR1 = ccr1 > 21000 ? 21000 : ccr1 < 11000 ? 11000 : ccr1;
				  TIM5->CCR2 = ccr2 > 21000 ? 21000 : ccr2 < 11000 ? 11000 : ccr2;
				  TIM5->CCR3 = ccr3 > 21000 ? 21000 : ccr3 < 11000 ? 11000 : ccr3;
				  TIM5->CCR4 = ccr4 > 21000 ? 21000 : ccr4 < 11000 ? 11000 : ccr4;
			  }
			  else
			  {
				  TIM5->CCR1 = 11000;
				  TIM5->CCR2 = 11000;
				  TIM5->CCR3 = 11000;
				  TIM5->CCR4 = 11000;
			  }

		  }
		  else
		  {
			  TIM5->CCR1 = 10500;
			  TIM5->CCR2 = 10500;
			  TIM5->CCR3 = 10500;
			  TIM5->CCR4 = 10500;
		  }
	  }
	  else
	  {
		  TIM5->CCR1 = 10500;
		  TIM5->CCR2 = 10500;
		  TIM5->CCR3 = 10500;
		  TIM5->CCR4 = 10500;
	  }

	  // Telemetry PID GAIN request
	  if(telemetry_rx_cplt_flag == 1)
	  {
		  telemetry_rx_cplt_flag = 0;

		  if(iBus.SwA == 1000)
		  {
			  unsigned char check_sum = 0xff;
			  for(int i = 0; i< 19; i++)
				  check_sum = check_sum - telemetry_rx_buf[i];

				  if(check_sum == telemetry_rx_buf[19])
				  {
					  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

					  TIM3->PSC = 1000;
					  HAL_Delay(100);

					  //TIM3 disable
					  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

					  switch(telemetry_rx_buf[2])
					  {
					  case 0:
						  roll.in.kp = *(float*)&telemetry_rx_buf[3];
						  roll.in.ki = *(float*)&telemetry_rx_buf[7];
						  roll.in.kd = *(float*)&telemetry_rx_buf[11];
						  EP_PIDGain_Write(telemetry_rx_buf[2], roll.in.kp, roll.in.ki, roll.in.kd);
						  EP_PIDGain_Read(telemetry_rx_buf[2], &roll.in.kp, &roll.in.ki, &roll.in.kd);
						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], roll.in.kp, roll.in.ki, roll.in.kd);
						  HAL_UART_Transmit_IT(&huart1, telemetry_tx_buf[0], 20);
						  break;
					  case 1:
						  roll.out.kp = *(float*)&telemetry_rx_buf[3];
						  roll.out.ki = *(float*)&telemetry_rx_buf[7];
						  roll.out.kd = *(float*)&telemetry_rx_buf[11];
						  EP_PIDGain_Write(telemetry_rx_buf[2], roll.out.kp, roll.out.ki, roll.out.kd);
						  EP_PIDGain_Read(telemetry_rx_buf[2], &roll.out.kp, &roll.out.ki, &roll.out.kd);
						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], roll.out.kp, roll.out.ki, roll.out.kd);
						  HAL_UART_Transmit_IT(&huart1, telemetry_tx_buf[0], 20);
						  break;
					  case 2:
						  pitch.in.kp = *(float*)&telemetry_rx_buf[3];
						  pitch.in.ki = *(float*)&telemetry_rx_buf[7];
						  pitch.in.kd = *(float*)&telemetry_rx_buf[11];
						  EP_PIDGain_Write(telemetry_rx_buf[2], pitch.in.kp, pitch.in.ki, pitch.in.kd);
						  EP_PIDGain_Read(telemetry_rx_buf[2], &pitch.in.kp, &pitch.in.ki, &pitch.in.kd);
						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], pitch.in.kp, pitch.in.ki, pitch.in.kd);
						  HAL_UART_Transmit_IT(&huart1, telemetry_tx_buf[0], 20);
						  break;
					  case 3:
						  pitch.out.kp = *(float*)&telemetry_rx_buf[3];
						  pitch.out.ki = *(float*)&telemetry_rx_buf[7];
						  pitch.out.kd = *(float*)&telemetry_rx_buf[11];
						  EP_PIDGain_Write(telemetry_rx_buf[2], pitch.out.kp, pitch.out.ki, pitch.out.kd);
						  EP_PIDGain_Read(telemetry_rx_buf[2], &pitch.out.kp, &pitch.out.ki, &pitch.out.kd);
						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], pitch.out.kp, pitch.out.ki, pitch.out.kd);
						  HAL_UART_Transmit_IT(&huart1, telemetry_tx_buf[0], 20);
						  break;
					  case 4:
						  yaw_heading.kp = *(float*)&telemetry_rx_buf[3];
						  yaw_heading.ki = *(float*)&telemetry_rx_buf[7];
						  yaw_heading.kd = *(float*)&telemetry_rx_buf[11];
						  EP_PIDGain_Write(telemetry_rx_buf[2], yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
						  EP_PIDGain_Read(telemetry_rx_buf[2], &yaw_heading.kp, &yaw_heading.ki, &yaw_heading.kd);
						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
						  HAL_UART_Transmit_IT(&huart1, telemetry_tx_buf[0], 20);
						  break;
					  case 5:
						  yaw_rate.kp = *(float*)&telemetry_rx_buf[3];
						  yaw_rate.ki = *(float*)&telemetry_rx_buf[7];
						  yaw_rate.kd = *(float*)&telemetry_rx_buf[11];
						  EP_PIDGain_Write(telemetry_rx_buf[2], yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
						  EP_PIDGain_Read(telemetry_rx_buf[2], &yaw_rate.kp, &yaw_rate.ki, &yaw_rate.kd);
						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
						  HAL_UART_Transmit_IT(&huart1, telemetry_tx_buf[0], 20);
						  break;
					  case 0x10:
						  switch(telemetry_rx_buf[3])
						  {
						  case 0:
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], roll.in.kp, roll.in.ki, roll.in.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  break;
						  case 1:
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], roll.out.kp, roll.out.ki, roll.out.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  break;
						  case 2:
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], pitch.in.kp, pitch.in.ki, pitch.in.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  break;
						  case 3:
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], pitch.out.kp, pitch.out.ki, pitch.out.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  break;
						  case 4:
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  break;
						  case 5:
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  break;
						  case 6:
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 0, roll.in.kp, roll.in.ki, roll.in.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 1, roll.out.kp, roll.out.ki, roll.out.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 2, pitch.in.kp, pitch.in.ki, pitch.in.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 3, pitch.out.kp, pitch.out.ki, pitch.out.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 4, yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 5, yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
							  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
							  break;
						  }
						  break;
					  }
				  }

		  }
	  }

	  //telemetry_transmit
	  if(tim7_20ms_flag ==1 && tim7_100ms_flag != 1)
	  {
		tim7_20ms_flag = 0;

		Encode_Msg_AHRS(&telemetry_tx_buf[0]);

		//HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 20);
		//HAL_Delay(15);

		//telemetry final UART1 PORT Transmit(20bytes)
		//nonblocking mode
		HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 20);
	  }

	  else if(tim7_20ms_flag == 1 && tim7_100ms_flag == 1)
	  {
		  tim7_20ms_flag = 0;
		  tim7_100ms_flag = 0;

		  Encode_Msg_AHRS(&telemetry_tx_buf[0]);
		  Encode_Msg_GPS(&telemetry_tx_buf[20]);

		  //telemetry final UART1 PORT Transmit(20bytes)
		  //nonblocking mode
		  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 40);

	  }

	  // Battery check value
	  batVolt = adcVal * 0.003619f;
	  //printf("%d\t %.2f\n", adcVal, batVolt);
	  //HAL_Delay(100);

	  // Battery check
	  if(batVolt < 11.2f)
	  {
		  low_bat_flag = 1;
	  }
	  else
	  {
		  low_bat_flag = 0;
	  }

	  //BNO080 data transmission
	  if(BNO080_dataAvailable() == 1)
	  {
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_0);

		  q[0] = BNO080_getQuatI();
		  q[1] = BNO080_getQuatJ();
		  q[2] = BNO080_getQuatK();
		  q[3] = BNO080_getQuatReal();
		  quatRadianAccuracy =BNO080_getQuatAccuracy();

		  Quaternion_Update(&q[0]);

		  BNO080_Roll = -BNO080_Roll;
		  BNO080_Pitch = -BNO080_Pitch;
		  //printf("%d,%d,%d\n", (int)(BNO080_Roll*100),(int)(BNO080_Pitch*100),(int)(BNO080_Yaw*100));
		  //printf("%.2f\t%.2f\t\n", BNO080_Roll, BNO080_Pitch);
		  //printf("%.2f\t", BNO080_Yaw);
	  }

	  //ICM20602 data transmission
	  if(ICM20602_DataReady() == 1)
	  {
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_1);

		  ICM20602_Get3AxisGyroRawData(&ICM20602.gyro_x_raw);

		  ICM20602.gyro_x = ICM20602.gyro_x_raw * 2000.f / 32768.f;
		  ICM20602.gyro_y = ICM20602.gyro_y_raw * 2000.f / 32768.f;
		  ICM20602.gyro_z = ICM20602.gyro_z_raw * 2000.f / 32768.f;

		  ICM20602.gyro_x = -ICM20602.gyro_x;
		  ICM20602.gyro_z = -ICM20602.gyro_z;
		  //printf("%d,%d,%d\n",(int)(ICM20602.gyro_x_raw),(int)(ICM20602.gyro_y_raw),(int)(ICM20602.gyro_z_raw));
		  //printf("%d,%d,%d\n",(int)(ICM20602.gyro_x * 100),(int)(ICM20602.gyro_y * 100),(int)(ICM20602.gyro_z * 100));
	  }

	  	//LPS22HH data transmission
		if(LPS22HH_DataReady() == 1)
		{
			LPS22HH_GetPressure(&LPS22HH.pressure_raw);
			LPS22HH_GetTemperature(&LPS22HH.temperature_raw);

			LPS22HH.baroAlt = getAltitude2(LPS22HH.pressure_raw/4096.f, LPS22HH.temperature_raw/100.f);


			//first pressure filter
			LPS22HH.baroAltFilt = LPS22HH.baroAltFilt * X +LPS22HH.baroAlt * (1.0f - X);
			//printf("%d,%d\n",(int)(LPS22HH.baroAlt * 100), (int)(LPS22HH.baroAltFilt * 100));
		}

	  //If GPS_UBX data[36 byte] completely, LED of FC is toggle
	  //M8N_UBX_NAV_POSLLH_Parsing that functions contained in the file M8N.h
	  if(m8n_rx_cplt_flag == 1)
	  {
		  m8n_rx_cplt_flag = 0;

		  if(M8N_UBX_CHKSUM_Check(&m8n_rx_buf[0], 36) == 1)
		  {
			  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);
			  M8N_UBX_NAV_POSLLH_Parsing(&m8n_rx_buf[0], &posllh);

			  //printf("LAT: %ld\t LON: %ld\t Height: %ld\t hAcc: %ld\t vAcc: %ld \r\n", posllh.lat, posllh.lon, posllh.height, posllh.hAcc, posllh.vAcc);
		  }
	  }

	  // iBus all transmission data check
	  if(ibus_rx_cplt_flag == 1)
	  {
		  ibus_rx_cplt_flag = 0;

		  // iBus CHECK SUM data check
		  if(iBus_Check_CHECKSUM(&ibus_rx_buf[0], 32) == 1)
		  {
			  //LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);

			  iBus_Parsing(&ibus_rx_buf[0], &iBus);
			  iBus_rx_cnt++;

			  if(iBus_isActiveFailsafe(&iBus) == 1)
			  {
				  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
				  //LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);
				  failsafe_flag = 1;
			  }
			  else
			  {
				  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
				  //LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_1);
				  failsafe_flag = 0;
			  }

			  // FS-i6 output data check
			  //printf("%d\t%d\t%d\t%d\t%d\t%d\n",
					  //iBus.RH, iBus.RV, iBus.LV, iBus.LH, iBus.SwA, iBus.SwC);
			  //HAL_Delay(30);
		  }
	  }

	  if(tim7_1000ms_flag == 1)
	  {
		  tim7_1000ms_flag = 0;
		  if(iBus_rx_cnt == 0)
		  {
			  failsafe_flag = 2;
		  }
		  iBus_rx_cnt = 0;
	  }
	  //if(failsafe_flag == 1 || failsafe_flag == 2 || low_bat_flag == 1 || iBus.SwC == 2000)
	  if(failsafe_flag == 1 || failsafe_flag == 2 || iBus.SwC == 2000)
	  {
		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  }
	  else
	  {
		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
// Motor safety mode function
int Is_iBus_Throttle_Min(void)
{
	if(ibus_rx_cplt_flag == 1)
	{
		ibus_rx_cplt_flag = 0;

		// iBus CHECK SUM data check
		if(iBus_Check_CHECKSUM(&ibus_rx_buf[0], 32) == 1)
		{
			iBus_Parsing(&ibus_rx_buf[0], &iBus);
			// Motor settings for safety
			if(iBus.LV < 1020)
				return 1;
		}
	}

	// If iBus data is not received, return is 0
	return 0;
}

// ESC calibration function
void ESC_Calibration(void)
{
	//ESC Calibration mode start
	//------------------------
	TIM5->CCR1 = 21000;
	TIM5->CCR2 = 21000;
	TIM5->CCR3 = 21000;
	TIM5->CCR4 = 21000;
	HAL_Delay(7000);

	TIM5->CCR1 = 10500;
	TIM5->CCR2 = 10500;
	TIM5->CCR3 = 10500;
	TIM5->CCR4 = 10500;
	HAL_Delay(8000);
	//------------------------
	//ESC Calibration mode end
}

// iBus input data check function
int Is_iBus_Received(void)
{
	if(ibus_rx_cplt_flag == 1)
	{
		ibus_rx_cplt_flag = 0;

		// iBus CHECK SUM data check
		if(iBus_Check_CHECKSUM(&ibus_rx_buf[0], 32) == 1)
		{
			iBus_Parsing(&ibus_rx_buf[0], &iBus);
			return 1;
		}
	}

	// If iBus data is not received, return is 0
	return 0;
}

void BNO080_Calibration(void)
{
	//Resets BNO080 to disable All output
	BNO080_Initialization();

	//BNO080/BNO085 Configuration
	//Enable dynamic calibration for accelerometer, gyroscope, and magnetometer
	//Enable Game Rotation Vector output
	//Enable Magnetic Field output
	BNO080_calibrateAll(); //Turn on cal for Accel, Gyro, and Mag
	BNO080_enableGameRotationVector(20000); //Send data update every 20ms (50Hz)
	BNO080_enableMagnetometer(20000); //Send data update every 20ms (50Hz)

	//Once magnetic field is 2 or 3, run the Save DCD Now command
  	printf("Calibrating BNO080. Pull up FS-i6 SWC to end calibration and save to flash\n");
  	printf("Output in form x, y, z, in uTesla\n\n");

	//while loop for calibration procedure
	//Iterates until iBus.SwC is mid point (1500)
	//Calibration procedure should be done while this loop is in iteration.
	while(iBus.SwC == 1500)
	{
		if(BNO080_dataAvailable() == 1)
		{
			//Observing the status bit of the magnetic field output
			float x = BNO080_getMagX();
			float y = BNO080_getMagY();
			float z = BNO080_getMagZ();
			unsigned char accuracy = BNO080_getMagAccuracy();

			float quatI = BNO080_getQuatI();
			float quatJ = BNO080_getQuatJ();
			float quatK = BNO080_getQuatK();
			float quatReal = BNO080_getQuatReal();
			unsigned char sensorAccuracy = BNO080_getQuatAccuracy();

			printf("%f,%f,%f,", x, y, z);
			if (accuracy == 0) printf("Unreliable\t");
			else if (accuracy == 1) printf("Low\t");
			else if (accuracy == 2) printf("Medium\t");
			else if (accuracy == 3) printf("High\t");

			printf("\t%f,%f,%f,%f,", quatI, quatI, quatI, quatReal);
			if (sensorAccuracy == 0) printf("Unreliable\n");
			else if (sensorAccuracy == 1) printf("Low\n");
			else if (sensorAccuracy == 2) printf("Medium\n");
			else if (sensorAccuracy == 3) printf("High\n");

			//Turn the LED and buzzer on when both accuracy and sensorAccuracy is high
			if(accuracy == 3 && sensorAccuracy == 3)
			{
				LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2);
				TIM3->PSC = 65000; //Very low frequency
				LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
			}
			else
			{
				LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2);
				LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
			}
		}

		Is_iBus_Received(); //Refreshes iBus Data for iBus.SwC
		HAL_Delay(100);
	}

	//Ends the loop when iBus.SwC is not mid point
	//Turn the LED and buzzer off
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2);
	LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	//Saves the current dynamic calibration data (DCD) to memory
	//Sends command to get the latest calibration status
	BNO080_saveCalibration();
	BNO080_requestCalibrationStatus();

	//Wait for calibration response, timeout if no response
	int counter = 100;
	while(1)
	{
		if(--counter == 0) break;
		if(BNO080_dataAvailable())
		{
			//The IMU can report many different things. We must wait
			//for the ME Calibration Response Status byte to go to zero
			if(BNO080_calibrationComplete() == 1)
			{
				printf("\nCalibration data successfully stored\n");
				LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
				TIM3->PSC = 2000;
				HAL_Delay(300);
				TIM3->PSC = 1500;
				HAL_Delay(300);
				LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
				HAL_Delay(1000);
				break;
			}
		}
		HAL_Delay(10);
	}
	if(counter == 0)
	{
		printf("\nCalibration data failed to store. Please try again.\n");
		LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		TIM3->PSC = 1500;
		HAL_Delay(300);
		TIM3->PSC = 2000;
		HAL_Delay(300);
		LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		HAL_Delay(1000);
	}

	//BNO080_endCalibration(); //Turns off all calibration
	//In general, calibration should be left on at all times. The BNO080
	//auto-calibrates and auto-records cal data roughly every 5 minutes

	//Resets BNO080 to disable Game Rotation Vector and Magnetometer
	//Enables Rotation Vector
	BNO080_Initialization();
	BNO080_enableRotationVector(2500); //Send data update every 2.5ms (400Hz)
}

//telemetry
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	static unsigned char cnt = 0;

	if(huart->Instance == USART1)
	{
		HAL_UART_Receive_IT(&huart1, &uart1_rx_data, 1);
		//HAL_UART_Transmit_IT(&huart1, &uart1_rx_data, 1);

		switch(cnt)
		{
		case 0:
			if(uart1_rx_data == 0x47)
			{
				telemetry_rx_buf[cnt] = uart1_rx_data;
				cnt++;
			}
			break;
		case 1:
			if(uart1_rx_data == 0x53)
			{
				telemetry_rx_buf[cnt] = uart1_rx_data;
				cnt++;
			}
			else
				cnt = 0;
			break;
		case 19:
			telemetry_rx_buf[cnt] = uart1_rx_data;
			cnt = 0;
			telemetry_rx_cplt_flag = 1;
			break;
		default:
			telemetry_rx_buf[cnt] = uart1_rx_data;
			cnt++;
			break;
		}
	}
}

void Encode_Msg_AHRS(unsigned char* telemetry_tx_buf)
{
	  //PID control
	  telemetry_tx_buf[0] = 0x46;
	  telemetry_tx_buf[1] = 0x43;

	  // AHRS data ID 0x10
	  telemetry_tx_buf[2] = 0x10;

	  telemetry_tx_buf[3] = (short)(BNO080_Roll*100); //LSB STORE
	  telemetry_tx_buf[4] = ((short)(BNO080_Roll*100))>>8; //MSB STORE (8BIT RIGHT SHIFT)

	  telemetry_tx_buf[5] = (short)(BNO080_Pitch*100);
	  telemetry_tx_buf[6] = ((short)(BNO080_Pitch*100))>>8;
//	  telemetry_tx_buf[5] = (short)(ICM20602.gyro_x*100);
//	  telemetry_tx_buf[6] = ((short)(ICM20602.gyro_x*100))>>8;

	  telemetry_tx_buf[7] = (unsigned short)(BNO080_Yaw*100);
	  telemetry_tx_buf[8] = ((unsigned short)(BNO080_Yaw*100))>>8;

	  //altitude air pressure sensor
	  telemetry_tx_buf[9] = (short)(LPS22HH.baroAltFilt*10);
	  telemetry_tx_buf[10] = ((short)(LPS22HH.baroAltFilt*10))>>8;


	  //iBus FLYSKY CONTROLLing machine

	  //ROLL TARGET
	  telemetry_tx_buf[11] = (short)((iBus.RH-1500)*0.1f*100); // min -50 ~ max +50 //LSB
	  telemetry_tx_buf[12] = ((short)((iBus.RH-1500)*0.1f*100))>>8; //MSB

	  //PITCH TARGET
	  telemetry_tx_buf[13] = (short)((iBus.RV-1500)*0.1f*100); // min -50 ~ max +50 //LSB
	  telemetry_tx_buf[14] = ((short)((iBus.RV-1500)*0.1f*100))>>8; //MSB

	  //YAW TARGET
	  telemetry_tx_buf[15] = (unsigned short)((iBus.LH-1000)*0.36f*100); // min 0~ max360 (angle) //LSB
	  telemetry_tx_buf[16] = ((unsigned short)((iBus.LH-1000)*0.36f*100))>>8; //MSB

	  //altitude TARGET (UNUSED)
	  telemetry_tx_buf[17] = (short)(iBus.LV*10);
	  telemetry_tx_buf[18] = ((short)(iBus.LV*10))>>8;

	  //Checksum
	  telemetry_tx_buf[19] = 0xff;

	  for(int i=0;i<19;i++) telemetry_tx_buf[19] = telemetry_tx_buf[19] - telemetry_tx_buf[i];
}

void Encode_Msg_GPS(unsigned char* telemetry_tx_buf)
{
	  telemetry_tx_buf[0] = 0x46;
	  telemetry_tx_buf[1] = 0x43;

	  // GPS data ID 0x11
	  telemetry_tx_buf[2] = 0x11;

	  // latitude of GPS
	  telemetry_tx_buf[3] = posllh.lat;
	  telemetry_tx_buf[4] = posllh.lat>>8;
	  telemetry_tx_buf[5] = posllh.lat>>16;
	  telemetry_tx_buf[6] = posllh.lat>>24;

	  // longitude of GPS
	  telemetry_tx_buf[7] = posllh.lon;
	  telemetry_tx_buf[8] = posllh.lon>>8;
	  telemetry_tx_buf[9] = posllh.lon>>16;
	  telemetry_tx_buf[10] = posllh.lon>>24;

	  //battery voltage information
	  telemetry_tx_buf[11] = (unsigned short)(batVolt*100);
	  telemetry_tx_buf[12] = ((unsigned short)(batVolt*100))>>8;

	  //iBus SwA SwC information (up/down)
	  telemetry_tx_buf[13] = iBus.SwA == 1000 ? 0 : 1;
	  telemetry_tx_buf[14] = iBus.SwC == 1000 ? 0 : iBus.SwC == 1500 ? 1 : 2;

	  telemetry_tx_buf[15] = failsafe_flag;

	  //UNUSED
	  telemetry_tx_buf[16] = 0x00;
	  telemetry_tx_buf[17] = 0x00;
	  telemetry_tx_buf[18] = 0x00;

	  //Checksum
	  telemetry_tx_buf[19] = 0xff;

	  for(int i=0;i<19;i++) telemetry_tx_buf[19] = telemetry_tx_buf[19] - telemetry_tx_buf[i];
}

void Encode_Msg_PID_Gain(unsigned char* telemetry_tx_buf, unsigned char id, float p, float i, float d)
{
	telemetry_tx_buf[0] = 0x46;
	telemetry_tx_buf[1] = 0x43;

	// GPS data ID 0x11
	telemetry_tx_buf[2] = id;

//	memcpy(%telemetry_tx_buf[3], &p, 4);
//	memcpy(%telemetry_tx_buf[7], &i, 4);
//	memcpy(%telemetry_tx_buf[11], &d, 4);

	*(float*)&telemetry_tx_buf[3] = p;
	*(float*)&telemetry_tx_buf[7] = i;
	*(float*)&telemetry_tx_buf[11] = d;

	//UNUSED
	telemetry_tx_buf[15] = 0x00;
	telemetry_tx_buf[16] = 0x00;
	telemetry_tx_buf[17] = 0x00;
	telemetry_tx_buf[18] = 0x00;

	//Checksum
	telemetry_tx_buf[19] = 0xff;

	for(int i=0;i<19;i++) telemetry_tx_buf[19] = telemetry_tx_buf[19] - telemetry_tx_buf[i];
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
