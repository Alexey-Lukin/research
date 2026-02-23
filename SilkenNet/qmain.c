/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Прошивка вузла КОРОЛЕВА (LoRa RX -> SIM7070G LTE-M -> Rails)
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
UART_HandleTypeDef huart1;  // Інтерфейс для модему SIM7070G
SUBGHZ_HandleTypeDef hsubghz;

/* USER CODE BEGIN PV */
// === ПАМ'ЯТЬ КОРОЛЕВИ ===
volatile uint8_t lora_rx_flag = 0;      // Прапорець: 1 - якщо щось прилетіло з лісу
uint8_t incoming_lora_payload[7];       // Буфер для 7 байтів від Солдата
uint32_t current_sender_id = 0;         // ID дерева (UID мікроконтролера)
int8_t current_rssi = 0;                // Рівень сигналу (щоб знати, як далеко дерево)

char at_tx_buffer[256];                 // Буфер для AT-команд
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SUBGHZ_Init(void);

/* USER CODE BEGIN PFP */
// Функції-обгортки для роботи з модемом
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
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init(); // Ініціалізуємо зв'язок з модемом (115200 baud)
  MX_SUBGHZ_Init();

  /* USER CODE BEGIN 2 */
  
  // 1. Ініціалізація Радіо (868 МГц)
  Radio.Init(NULL);
  Radio.SetChannel(868000000); 
  
  // 2. Ініціалізація Модему SIM7070G
  // (Надсилаємо команду перевірки готовності та налаштовуємо APN)
  SIM7070_SendATCommand("AT\r\n", 500);
  SIM7070_SendATCommand("AT+CNMP=38\r\n", 1000); // Режим LTE Only (NB-IoT/LTE-M)
  // Тут буде ваша AT-команда для налаштування APN мобільного оператора:
  // SIM7070_SendATCommand("AT+CNACT=0,1\r\n", 3000); 

  // 3. Відкриваємо вуха: Переводимо радіомодуль у режим безперервного слухання
  // 0xFFFFFF означає нескінченний таймаут (поки не вимкнемо живлення)
  Radio.Rx(0xFFFFFF); 
  
  /* USER CODE END 2 */

  /* Infinite loop */
  while (1)
  {
    /* USER CODE BEGIN WHILE */
    
    // =========================================================================
    // ФАЗА ОЧІКУВАННЯ ТА ТРАНЗИТУ
    // =========================================================================
    
    // Якщо апаратне переривання радіомодуля спіймало пакет
    if (lora_rx_flag == 1) 
    {
        // 1. Формуємо та відправляємо JSON на ваш Rails-сервер
        Send_Data_To_Rails(current_sender_id, incoming_lora_payload, current_rssi);
        
        // 2. Очищаємо прапорець
        lora_rx_flag = 0;
        
        // 3. Знову переводимо радіо в режим прийому, бо під час обробки
        //    трансивер міг перейти в режим очікування
        Radio.Rx(0xFFFFFF); 
    }
    
    // Королева не спить (HAL_SuspendTick тут не викликається).
    // Вона працює від великої сонячної панелі.
    
    /* USER CODE END WHILE */
  }
}

/* USER CODE BEGIN 4 */

// =========================================================================
// АПАРАТНИЙ РЕФЛЕКС РАДІО (Вуха Королеви)
// =========================================================================
// Ця функція викликається автоматично, коли трансивер ловить валідний пакет
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    // Якщо розмір співпадає з нашими 7 байтами від Солдата
    if (size == 7) 
    {
        // Копіюємо байти в пам'ять Королеви
        memcpy(incoming_lora_payload, payload, 7);
        current_rssi = (int8_t)rssi;
        
        // У реальності тут ще буде зчитування Device EUI з пакета, 
        // але для прототипу генеруємо умовний ID на базі перших байтів
        current_sender_id = (payload[0] << 8) | payload[1]; 

        // Підіймаємо прапорець для головного циклу
        lora_rx_flag = 1; 
    }
}

// =========================================================================
// ДРАЙВЕР СТІЛЬНИКОВОГО МОДЕМУ (SIM7070G)
// =========================================================================

// Проста обгортка для відправки AT-команд через UART
void SIM7070_SendATCommand(char* command, uint32_t delay_ms)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)command, strlen(command), 1000);
    HAL_Delay(delay_ms); // Чекаємо на відповідь від модему (OK)
}

// Функція конвертації фізики в JSON та відправки HTTP POST
void Send_Data_To_Rails(uint32_t device_id, uint8_t* payload, int8_t rssi)
{
    // 1. Формуємо тіло JSON
    char json_body[128];
    sprintf(json_body, "{\"device_id\":%lu,\"rssi\":%d,\"raw_data\":\"%02x%02x%02x%02x%02x%02x%02x\"}", 
            device_id, rssi,
            payload[0], payload[1], payload[2], payload[3], 
            payload[4], payload[5], payload[6]);

    // 2. Ініціалізуємо HTTP запит (команди специфічні для SIM7070G)
    SIM7070_SendATCommand("AT+SHCONF=\"URL\",\"http://api.silkennet.com/v1/telemetry\"\r\n", 500);
    SIM7070_SendATCommand("AT+SHCONF=\"BODYLEN\",1024\r\n", 100);
    SIM7070_SendATCommand("AT+SHCONF=\"HEADERLEN\",256\r\n", 100);
    SIM7070_SendATCommand("AT+SHCONN\r\n", 3000); // Встановлюємо з'єднання
    
    // 3. Завантажуємо JSON у пам'ять модему
    sprintf(at_tx_buffer, "AT+SHBOD=%d,10000\r\n", strlen(json_body));
    SIM7070_SendATCommand(at_tx_buffer, 100);
    SIM7070_SendATCommand(json_body, 500);

    // 4. Робимо POST запит (Action 3 = POST)
    SIM7070_SendATCommand("AT+SHREQ=\"/v1/telemetry\",3\r\n", 2000);

    // 5. Закриваємо з'єднання
    SIM7070_SendATCommand("AT+SHDISC\r\n", 500);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
