/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Прошивка вузла КОРОЛЕВА (LoRa RX -> AES-128 Decrypt -> SIM7070G LTE-M -> Rails)
  * @processor      : STM32WLE5JC
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>
#include "radio.h" // Низькорівневий драйвер LoRa

/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;  
SUBGHZ_HandleTypeDef hsubghz;
CRYP_HandleTypeDef hcryp; // ДОДАНО: Апаратний криптопроцесор AES-128

/* USER CODE BEGIN PV */
// === 0. КЛЮЧІ ОХОРОНИ (Trading Post) ===
// Цей ключ МАЄ БУТИ ІДЕНТИЧНИМ ключу у Солдаті
uint32_t aes_key[4] = {0x2B7E1516, 0x28AED2A6, 0xABF71588, 0x09CF4F3C};

// === ПАМ'ЯТЬ КОРОЛЕВИ ===
volatile uint8_t lora_rx_flag = 0;      
uint8_t incoming_lora_payload[16]; // Змінено на 16 байтів для AES-блоку
uint8_t decrypted_payload[16];     // Буфер для розшифрованих даних
uint32_t current_sender_id = 0;         
int8_t current_rssi = 0;                

char at_tx_buffer[256];                 
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SUBGHZ_Init(void);
static void MX_CRYP_Init(void); // ДОДАНО: Ініціалізація шифрування

/* USER CODE BEGIN PFP */
void SIM7070_WakeUp(void);
void SIM7070_SendATCommand(char* command, uint32_t delay_ms);
void Send_Data_To_Rails(uint32_t device_id, uint8_t* payload, int8_t rssi);
/* USER CODE END PFP */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init(); 
  MX_SUBGHZ_Init();
  MX_CRYP_Init(); // Вмикаємо апаратний AES

  /* USER CODE BEGIN 2 */
  Radio.Init(NULL);
  Radio.SetChannel(868000000); 
  
  SIM7070_SendATCommand("AT\r\n", 500);
  SIM7070_SendATCommand("AT+CNMP=38\r\n", 1000); 

  Radio.Rx(0xFFFFFF); 
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN WHILE */
    
    if (lora_rx_flag == 1) 
    {
        // 1. Розшифровуємо 16 байт
        HAL_CRYP_Decrypt(&hcryp, (uint32_t*)incoming_lora_payload, 4, (uint32_t*)decrypted_payload, 1000);

        // 2. Витягуємо ID Солдата з перших 4 байтів розшифрованого пакета
        current_sender_id = (decrypted_payload[0] << 24) | (decrypted_payload[1] << 16) | (decrypted_payload[2] << 8) | decrypted_payload[3];

        // 3. Формуємо та відправляємо JSON
        Send_Data_To_Rails(current_sender_id, decrypted_payload, current_rssi);
        
        lora_rx_flag = 0;
        Radio.Rx(0xFFFFFF); 
    }
    
    /* USER CODE END WHILE */
  }
}

/* USER CODE BEGIN 4 */

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    // Очікуємо 16 байт
    if (size == 16) 
    {
        memcpy(incoming_lora_payload, payload, 16);
        current_rssi = (int8_t)rssi;
        lora_rx_flag = 1; 
    }
}

void SIM7070_SendATCommand(char* command, uint32_t delay_ms)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)command, strlen(command), 1000);
    HAL_Delay(delay_ms); 
}

void Send_Data_To_Rails(uint32_t device_id, uint8_t* payload, int8_t rssi)
{
    char json_body[256];
    // Форматуємо 16 байтів у hex-рядок
    sprintf(json_body, "{\"device_id\":%lu,\"rssi\":%d,\"raw_data\":\"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\"}", 
            device_id, rssi,
            payload[0], payload[1], payload[2], payload[3], 
            payload[4], payload[5], payload[6], payload[7],
            payload[8], payload[9], payload[10], payload[11],
            payload[12], payload[13], payload[14], payload[15]);

    SIM7070_SendATCommand("AT+SHCONF=\"URL\",\"http://api.silkennet.com/v1/telemetry\"\r\n", 500);
    SIM7070_SendATCommand("AT+SHCONF=\"BODYLEN\",1024\r\n", 100);
    SIM7070_SendATCommand("AT+SHCONF=\"HEADERLEN\",256\r\n", 100);
    SIM7070_SendATCommand("AT+SHCONN\r\n", 3000); 
    
    sprintf(at_tx_buffer, "AT+SHBOD=%d,10000\r\n", strlen(json_body));
    SIM7070_SendATCommand(at_tx_buffer, 100);
    SIM7070_SendATCommand(json_body, 500);

    SIM7070_SendATCommand("AT+SHREQ=\"/v1/telemetry\",3\r\n", 2000);
    SIM7070_SendATCommand("AT+SHDISC\r\n", 500);
}

// Функція конфігурації апаратного AES
static void MX_CRYP_Init(void)
{
  hcryp.Instance = AES;
  hcryp.Init.DataType = CRYP_DATATYPE_32B;
  hcryp.Init.KeySize = CRYP_KEYSIZE_128B;
  hcryp.Init.pKey = aes_key;
  hcryp.Init.Algorithm = CRYP_AES_ECB; 
  HAL_CRYP_Init(&hcryp);
}

/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
