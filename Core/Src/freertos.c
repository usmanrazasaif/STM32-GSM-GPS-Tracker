/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gps.h"
#include "string.h"
#include "semphr.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    NETWORK_NO_SIGNAL = 0,
    NETWORK_CONNECTED = 1,
	NETWORK_DISCONNECTED = 2
} NetworkState_t;

typedef enum {
    DEVICE_IDLE = 0,
    DEVICE_POSTING_HTTP_DATA = 1,
	DEVICE_SENDING_SMS = 2
} DeviceState_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

DeviceState_t Device_State = DEVICE_IDLE;

int smv = 0;
uint8_t tx;
uint8_t uartbuff[100];
uint8_t uartbuffindex = 0;
uint8_t is_network_connected = 0;

float askari_10[2] = {31.535449, 74.417237};
int send_text = 0;
int location_sent = 0;
int location_posted = 0;
int text_message_sent = 0;
int posting_location = 0;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for SMS_Task */
osThreadId_t SMS_TaskHandle;
const osThreadAttr_t SMS_Task_attributes = {
  .name = "SMS_Task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for HTTP_Task */
osThreadId_t HTTP_TaskHandle;
const osThreadAttr_t HTTP_Task_attributes = {
  .name = "HTTP_Task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for SIM_Ping */
osThreadId_t SIM_PingHandle;
const osThreadAttr_t SIM_Ping_attributes = {
  .name = "SIM_Ping",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for send_sms_semaphore */
osSemaphoreId_t send_sms_semaphoreHandle;
osSemaphoreId_t post_location_semaphore;
osSemaphoreId_t ping_modem_semaphore;
const osSemaphoreAttr_t send_sms_semaphore_attributes = {
  .name = "send_sms_semaphore"
};
const osSemaphoreAttr_t post_location_semaphore_attributes = {
  .name = "post_location_semaphore"
};
const osSemaphoreAttr_t ping_modem_semaphore_attributes = {
  .name = "ping_modem_semaphore"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{

	if(huart == &huart2) GPS_UART_CallBack();

	if(huart == &huart6)
	{
		uartbuff[uartbuffindex++] = tx;
		if (uartbuffindex > 99) uartbuffindex = 0;
		HAL_UART_Receive_IT(&huart6, &tx, 1);

		if (strstr((char*)uartbuff, "CMT") != NULL)
		{
			send_text = 1;
			// Give the semaphore to unblock the task
			xSemaphoreGiveFromISR(send_sms_semaphoreHandle, NULL);
			HAL_UART_Transmit(&huart3, (uint8_t *) "MSG RCVD \n", 11, HAL_MAX_DELAY);
		}

		if (strstr((char*)uartbuff, "CREG: 0,1") != NULL)
		{
			is_network_connected = 1;
		}
		else if (strstr((char*)uartbuff, "CREG: 0,2") != NULL)
		{
			is_network_connected = 2;
		}
	}
}

void SIM_SendCmd(char* cmd)
{
  HAL_UART_Transmit_IT(&huart6, (uint8_t*)cmd, strlen(cmd));
}


void SIM_PostData(float latti, float longi)
{
	Device_State = DEVICE_POSTING_HTTP_DATA;
	SIM_SendCmd("AT+CREG?\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));

	if(is_network_connected != 1) return;

	char cmdi[256];
	if(latti > 0.0 && longi > 0.0)
		sprintf(cmdi, "{\"location\":{\"value\":1,\"context\":{\"lat\":%f,\"lng\":%f}}}\r\n", latti, longi);
	else
		sprintf(cmdi, "{\"location\":{\"value\":1,\"context\":{\"lat\":31.47557,\"lng\":74.34190}}}\r\n");


	SIM_SendCmd("AT+CREG?\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+CSQ\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+CGATT=1\r\n");
	vTaskDelay(pdMS_TO_TICKS(1000));
	SIM_SendCmd("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+SAPBR=3,1,\"APN\",\"ufone.pinternet\"\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+SAPBR=1,1\r\n");
	vTaskDelay(pdMS_TO_TICKS(1000));
	SIM_SendCmd("AT+SAPBR=2,1\r\n");
	vTaskDelay(pdMS_TO_TICKS(1000));
	SIM_SendCmd("AT+HTTPINIT\r\n");
	vTaskDelay(pdMS_TO_TICKS(1000));
	SIM_SendCmd("AT+HTTPPARA=\"CID\",1\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+HTTPPARA=\"URL\",\"industrial.api.ubidots.com/api/v1.6/devices/vhtracker\"\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+HTTPPARA=\"USERDATA\",\"X-Auth-Token: BBUS-57yGDiCc5NPs7gMwJdQAhLdp3SoJBU\"\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+HTTPDATA=200,3000\r\n");
	vTaskDelay(pdMS_TO_TICKS(1000));
	//SIM_SendCmd("{\"location\":{\"value\":1,\"context\":{\"lat\":37.773,\"lng\":71.431}}}\r\n");
	SIM_SendCmd(cmdi);
	vTaskDelay(pdMS_TO_TICKS(3000));
	SIM_SendCmd("AT+HTTPACTION=1\r\n");
	vTaskDelay(pdMS_TO_TICKS(5000));
	SIM_SendCmd("AT+HTTPREAD\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));
	SIM_SendCmd("AT+HTTPTERM\r\n");
	vTaskDelay(pdMS_TO_TICKS(500));

	location_sent++;
	Device_State = DEVICE_IDLE;

}


void SIM_SendTextMessage(char* phoneNumber, char* message)
{
  char cmd[160];
  Device_State = DEVICE_SENDING_SMS;

  SIM_SendCmd("AT+CREG?\r\n");
  vTaskDelay(pdMS_TO_TICKS(1000));

  if(is_network_connected == 1)
  {
	  SIM_SendCmd("AT+CMGF=1\r\n");  // Set SMS text mode
	  vTaskDelay(pdMS_TO_TICKS(1000));
	  SIM_SendCmd("AT+CSCS=\"GSM\"\r\n");  // Set character set to GSM
	  vTaskDelay(pdMS_TO_TICKS(1000));

	  // Set the recipient phone number
	  sprintf(cmd, "AT+CMGS=\"%s\"\r\n", phoneNumber);
	  SIM_SendCmd(cmd);
	  vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for the prompt '>'

	  // Send the SMS message
	  sprintf(cmd, "%s%c", message, 26);  // 26 is the ASCII code for Ctrl+Z
	  SIM_SendCmd(cmd);
	  vTaskDelay(pdMS_TO_TICKS(5000));  // Wait for the message to be sent

	  text_message_sent++;
  }

  Device_State = DEVICE_IDLE;
}

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void SMS_Task_Function(void *argument);
void HTTP_Task_Function(void *argument);
void Sim_Ping_Task(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of send_sms_semaphore */
  send_sms_semaphoreHandle = osSemaphoreNew(1, 0, &send_sms_semaphore_attributes);
  post_location_semaphore = osSemaphoreNew(1, 0, &post_location_semaphore_attributes);
  ping_modem_semaphore = osSemaphoreNew(1, 0, &ping_modem_semaphore_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of SMS_Task */
  SMS_TaskHandle = osThreadNew(SMS_Task_Function, NULL, &SMS_Task_attributes);

  /* creation of HTTP_Task */
  HTTP_TaskHandle = osThreadNew(HTTP_Task_Function, NULL, &HTTP_Task_attributes);

  /* creation of SIM_Ping */
  SIM_PingHandle = osThreadNew(Sim_Ping_Task, NULL, &SIM_Ping_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  GPS_Init();
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_SMS_Task_Function */
/**
* @brief Function implementing the SMS_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_SMS_Task_Function */
void SMS_Task_Function(void *argument)
{
  /* USER CODE BEGIN SMS_Task_Function */
  /* Infinite loop */
  for(;;)
  {
	 // Wait indefinitely for the semaphore
	if (xSemaphoreTake(send_sms_semaphoreHandle, portMAX_DELAY) == pdTRUE)
	{
	  // Task code to run once upon receiving UART data
		vTaskSuspend(SIM_PingHandle);
		vTaskSuspend(HTTP_TaskHandle);
		char url[100];

		if(GPS_LAT() > 0.0 && GPS_LNG() > 0.0)
			sprintf(url, "https://maps.google.com/?q=%f,%f", GPS_LAT(), GPS_LNG());
		else
			sprintf(url, "https://maps.google.com/?q=%f,%f", 31.47557, 74.34190);

		SIM_SendTextMessage("+923164044136", url);
		send_text = 0;
		memset(uartbuff, 0, 100);
		uartbuffindex = 0;
		vTaskResume(SIM_PingHandle);
		vTaskResume(HTTP_TaskHandle);
	}
    osDelay(1000);
  }
  /* USER CODE END SMS_Task_Function */
}

/* USER CODE BEGIN Header_HTTP_Task_Function */
/**
* @brief Function implementing the HTTP_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_HTTP_Task_Function */
void HTTP_Task_Function(void *argument)
{
  /* USER CODE BEGIN HTTP_Task_Function */
  /* Infinite loop */
  for(;;)
  {
	vTaskSuspend(SIM_PingHandle);
	vTaskSuspend(SMS_TaskHandle);
	SIM_PostData(GPS_LAT(), GPS_LNG());
	vTaskResume(SIM_PingHandle);
	vTaskResume(SMS_TaskHandle);
	vTaskDelay(pdMS_TO_TICKS(30000));
  }
  /* USER CODE END HTTP_Task_Function */
}

/* USER CODE BEGIN Header_Sim_Ping_Task */
/**
* @brief Function implementing the SIM_Ping thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Sim_Ping_Task */
void Sim_Ping_Task(void *argument)
{
  /* USER CODE BEGIN Sim_Ping_Task */
	HAL_UART_Receive_IT(&huart6, &tx, 1);
	char usb_buff[100];
  /* Infinite loop */
  for(;;)
  {
	HAL_UART_Transmit_IT(&huart6, (uint8_t *)"AT+CREG?\r\n", 10);
	Device_State = DEVICE_IDLE;
	sprintf(usb_buff, "Connection Status: %d \n", is_network_connected);
	HAL_UART_Transmit(&huart3, (uint8_t *)usb_buff, 23, HAL_MAX_DELAY);
    //osDelay(pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  /* USER CODE END Sim_Ping_Task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

