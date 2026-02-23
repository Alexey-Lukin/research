/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Прошивка вузла КОРОЛЕВА (LoRa RX -> Reflex OTA -> CIFO Cache -> Batch LTE-M)
  * @processor      : STM32WLE5JC
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

// Підключаємо низькорівневий драйвер радіо (Radio Middleware)
#include "radio.h" 
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;  // Інтерфейс для модему SIM7070G (LTE-M)
SUBGHZ_HandleTypeDef hsubghz;
CRYP_HandleTypeDef hcryp; // Апаратний криптопроцесор AES-128

/* USER CODE BEGIN PV */

// =========================================================================
// === 0. КЛЮЧІ ОХОРОНИ (Trading Post) ===
// =========================================================================
// Секретний 128-бітний ключ мережі Silken Net. 
// МАЄ БУТИ ІДЕНТИЧНИМ ключу, зашитому в усіх Солдатах.
uint32_t aes_key[4] = {0x2B7E1516, 0x28AED2A6, 0xABF71588, 0x09CF4F3C};

// =========================================================================
// === 1. ПАМ'ЯТЬ КОРОЛЕВИ (Прийом Даних) ===
// =========================================================================
volatile uint8_t lora_rx_flag = 0;      // Прапорець: 1 - пакет спіймано
uint8_t incoming_lora_payload[16];      // Сирий 16-байтний зашифрований пакет
uint8_t decrypted_payload[16];          // Розшифрований пакет від Солдата
int8_t current_rssi = 0;                // Рівень сигналу

char at_tx_buffer[256];                 // Буфер для формування AT-команд

// =========================================================================
// === 1.5. EDGE КЕШУВАННЯ (CIFO & Дедуплікація) ===
// =========================================================================
#define CACHE_MAX_ENTRIES 50 // Максимальна місткість нашого кешу

typedef struct {
    uint32_t uid;               // ID дерева
    uint8_t payload[16];        // Останні розшифровані дані
    int8_t rssi;                // Сила сигналу
    uint8_t is_active;          // 1 - якщо слот зайнятий
} EdgeCache;

EdgeCache forest_cache[CACHE_MAX_ENTRIES];
uint8_t cache_count = 0;

char json_buffer[8192]; // Великий буфер для пакетної відправки (Batching)

// =========================================================================
// === 2. БУНКЕР OTA-ОНОВЛЕНЬ (Передача нових контрактів) ===
// =========================================================================
// Прапорець: 1 - якщо ми зараз в процесі роздачі нової прошивки лісу
uint8_t ota_is_active = 1; 
uint8_t current_ota_chunk_idx = 0;

// Уявімо, що це новий контракт bio_contract_v2.rb (байт-код mruby)
// На практиці Королева може отримувати його з Rails-сервера і складати сюди.
uint8_t pending_ota_bytecode[] = {
  0x52, 0x49, 0x54, 0x45, 0x30, 0x33, 0x30, 0x30, 0x00, 0x00,
  0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44,
  0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44,
  0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD
};
uint16_t pending_ota_size = sizeof(pending_ota_bytecode);

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SUBGHZ_Init(void);
static void MX_CRYP_Init(void); // Ініціалізація шифрування

/* USER CODE BEGIN PFP */
// Функції-обгортки для роботи з модемом та транзитом
void SIM7070_SendATCommand(char* command, uint32_t delay_ms);
void Process_And_Cache_Data(uint32_t uid, uint8_t* payload, int8_t rssi);
void Flush_Cache_To_Rails(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init(); // UART для розмови з SIM7070G (115200 baud)
  MX_SUBGHZ_Init();
  MX_CRYP_Init();        // Вмикаємо апаратний модуль AES-128

  /* USER CODE BEGIN 2 */
  
  // 1. Ініціалізація низькорівневого радіо
  Radio.Init(NULL);
  Radio.SetChannel(868000000); // 868 МГц (Європа / Україна)
  
  // 2. Ініціалізація Кешу нулями
  memset(forest_cache, 0, sizeof(forest_cache));

  // 3. Ініціалізація модему SIM7070G
  // Перевіряємо зв'язок та налаштовуємо режим (LTE-M / NB-IoT)
  SIM7070_SendATCommand("AT\r\n", 500);
  SIM7070_SendATCommand("AT+CNMP=38\r\n", 1000); 

  // 4. Відкриваємо вуха: Королева переходить у режим безперервного слухання
  // 0xFFFFFF = нескінченний таймаут
  Radio.Rx(0xFFFFFF); 
  
  /* USER CODE END 2 */

  uint32_t last_flush_time = HAL_GetTick();

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // =========================================================================
    // ФАЗА ОЧІКУВАННЯ ТА ОБРОБКИ РАДІОЕФІРУ
    // =========================================================================
    
    // Якщо апаратне переривання OnRxDone спіймало пакет від Солдата
    if (lora_rx_flag == 1) 
    {
        // 1. РОЗШИФРОВУЄМО ПАКЕТ
        // Розшифровуємо 4 слова (16 байт) апаратним модулем
        HAL_CRYP_Decrypt(&hcryp, (uint32_t*)incoming_lora_payload, 4, (uint32_t*)decrypted_payload, 1000);

        // =========================================================================
        // РЕФЛЕКТОРНИЙ ПОСТРІЛ (OTA BROADCAST)
        // Солдат прямо зараз (після відправки) слухає ефір рівно 500 мс.
        // Ми маємо блискавично вистрілити шматком нової прошивки йому у відповідь.
        // =========================================================================
        if (ota_is_active) {
            uint8_t ota_chunk[16] = {0};
            uint8_t encrypted_ota[16] = {0};
            
            // В один 16-байтний пакет влазить 13 байт чистого коду (3 байти - заголовок)
            uint8_t total_chunks = (pending_ota_size + 12) / 13; 

            // Формуємо заголовок (0x99 = маркер OTA-пакета)
            ota_chunk[0] = 0x99;
            ota_chunk[1] = current_ota_chunk_idx;
            ota_chunk[2] = total_chunks;

            // Копіюємо до 13 байт коду в пакет
            uint16_t offset = current_ota_chunk_idx * 13;
            uint8_t bytes_to_copy = (pending_ota_size - offset > 13) ? 13 : (pending_ota_size - offset);
            memcpy(&ota_chunk[3], &pending_ota_bytecode[offset], bytes_to_copy);

            // Шифруємо цей шматок коду
            HAL_CRYP_Encrypt(&hcryp, (uint32_t*)ota_chunk, 4, (uint32_t*)encrypted_ota, 1000);

            // СТРІЛЯЄМО В ЕФІР
            Radio.Send(encrypted_ota, 16);
            
            // Даємо радіомодулю час фізично передати пакет (бл. 50-60 мс)
            HAL_Delay(60); 

            // Перемикаємося на наступний шматок для наступного дерева
            current_ota_chunk_idx++;
            if (current_ota_chunk_idx >= total_chunks) {
                current_ota_chunk_idx = 0; 
                // Якщо маємо оновити ліс лише один раз, розкоментувати:
                // ota_is_active = 0;
            }
        }

        // =========================================================================
        // ОБРОБКА ДАНИХ (КЕШУВАННЯ)
        // =========================================================================
        // Витягуємо унікальний ID Солдата (перші 4 байти)
        uint32_t sender_id = (decrypted_payload[0] << 24) | (decrypted_payload[1] << 16) | (decrypted_payload[2] << 8) | decrypted_payload[3];
        
        // Замість миттєвої відправки, складаємо в CIFO-кеш
        Process_And_Cache_Data(sender_id, decrypted_payload, current_rssi);
        
        // Очищаємо прапорець і знову відкриваємо вуха
        lora_rx_flag = 0;
        Radio.Rx(0xFFFFFF); 
    }
    
    // =========================================================================
    // СКИДАННЯ КЕШУ НА СЕРВЕР (GCCS Batching)
    // =========================================================================
    // Відправляємо пакет даних, якщо кеш заповнений майже повністю (залишилось 5 вільних слотів)
    // АБО пройшло достатньо часу (наприклад, 1 година = 3 600 000 мс)
    if (cache_count >= (CACHE_MAX_ENTRIES - 5) || (HAL_GetTick() - last_flush_time > 3600000)) {
        if (cache_count > 0) {
            Flush_Cache_To_Rails();
            last_flush_time = HAL_GetTick(); // Оновлюємо таймер
        }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

// =========================================================================
// АПАРАТНИЙ РЕФЛЕКС РАДІО (Вуха Королеви)
// =========================================================================
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    // Очікуємо рівно 16 байт (повний зашифрований блок AES-128)
    if (size == 16) 
    {
        memcpy(incoming_lora_payload, payload, 16);
        current_rssi = (int8_t)rssi;
        lora_rx_flag = 1; // Сигналізуємо головному циклу
    }
}

// =========================================================================
// ЛОГІКА КЕШУ (Дедуплікація та CIFO)
// =========================================================================
void Process_And_Cache_Data(uint32_t uid, uint8_t* payload, int8_t rssi)
{
    // 1. ДЕДУПЛІКАЦІЯ: Шукаємо, чи є вже це дерево в кеші
    for(int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if(forest_cache[i].is_active && forest_cache[i].uid == uid) {
            // Оновлюємо дані на найсвіжіші (бо дерево могло надіслати новий статус)
            memcpy(forest_cache[i].payload, payload, 16);
            forest_cache[i].rssi = rssi;
            return;
        }
    }

    // 2. ВСТАВКА: Якщо є вільне місце в кеші
    if(cache_count < CACHE_MAX_ENTRIES) {
        for(int i = 0; i < CACHE_MAX_ENTRIES; i++) {
            if(!forest_cache[i].is_active) {
                forest_cache[i].uid = uid;
                memcpy(forest_cache[i].payload, payload, 16);
                forest_cache[i].rssi = rssi;
                forest_cache[i].is_active = 1;
                cache_count++;
                return;
            }
        }
    } 
    // 3. CIFO (Closest In Farthest Out): Кеш повний, викидаємо "найдальшого"
    else {
        int farthest_idx = 0;
        int8_t min_rssi = 127; // Початкове значення - максимально можливий сигнал

        // Шукаємо пакет з найгіршим сигналом (найдальше дерево)
        for(int i = 0; i < CACHE_MAX_ENTRIES; i++) {
            if(forest_cache[i].rssi < min_rssi) {
                min_rssi = forest_cache[i].rssi;
                farthest_idx = i;
            }
        }

        // Перезаписуємо найдальше дерево новими критичними даними
        forest_cache[farthest_idx].uid = uid;
        memcpy(forest_cache[farthest_idx].payload, payload, 16);
        forest_cache[farthest_idx].rssi = rssi;
    }
}

// =========================================================================
// ПАКЕТНЕ ВІДПРАВЛЕННЯ ЧЕРЕЗ LTE-M (GCCS)
// =========================================================================
void Flush_Cache_To_Rails(void)
{
    // Будуємо великий JSON-масив об'єктів
    strcpy(json_buffer, "[");
    
    uint8_t is_first = 1;
    char temp_obj[256];

    for(int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if(forest_cache[i].is_active) {
            if (!is_first) { strcat(json_buffer, ","); }
            
            sprintf(temp_obj, "{\"id\":%lu,\"rssi\":%d,\"data\":\"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\"}", 
                    forest_cache[i].uid, forest_cache[i].rssi,
                    forest_cache[i].payload[0], forest_cache[i].payload[1], forest_cache[i].payload[2], forest_cache[i].payload[3], 
                    forest_cache[i].payload[4], forest_cache[i].payload[5], forest_cache[i].payload[6], forest_cache[i].payload[7],
                    forest_cache[i].payload[8], forest_cache[i].payload[9], forest_cache[i].payload[10], forest_cache[i].payload[11],
                    forest_cache[i].payload[12], forest_cache[i].payload[13], forest_cache[i].payload[14], forest_cache[i].payload[15]);
            
            strcat(json_buffer, temp_obj);
            
            // Звільняємо слот після пакування
            forest_cache[i].is_active = 0;
            is_first = 0;
        }
    }
    strcat(json_buffer, "]");
    cache_count = 0;

    // Ініціалізація HTTP (специфічно для модуля SIM7070G)
    SIM7070_SendATCommand("AT+SHCONF=\"URL\",\"http://api.silkennet.com/v1/telemetry/batch\"\r\n", 500);
    SIM7070_SendATCommand("AT+SHCONF=\"BODYLEN\",8192\r\n", 100);
    SIM7070_SendATCommand("AT+SHCONN\r\n", 3000); // Відкриття з'єднання
    
    // Встановлення тіла запиту
    sprintf(at_tx_buffer, "AT+SHBOD=%d,10000\r\n", strlen(json_buffer));
    SIM7070_SendATCommand(at_tx_buffer, 100);
    SIM7070_SendATCommand(json_buffer, 1000); // Передача JSON

    // Виконання POST запиту (Дія 3)
    SIM7070_SendATCommand("AT+SHREQ=\"/v1/telemetry/batch\",3\r\n", 2000);
    
    // Розрив з'єднання
    SIM7070_SendATCommand("AT+SHDISC\r\n", 500);
}

// =========================================================================
// ДРАЙВЕР СТІЛЬНИКОВОГО МОДЕМУ (SIM7070G)
// =========================================================================
// Проста обгортка для відправки AT-команд через UART
void SIM7070_SendATCommand(char* command, uint32_t delay_ms)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)command, strlen(command), 1000);
    HAL_Delay(delay_ms); // Чекаємо на відповідь (OK)
}

// =========================================================================
// ІНІЦІАЛІЗАЦІЯ КРИПТОГРАФІЇ
// =========================================================================
static void MX_CRYP_Init(void)
{
  hcryp.Instance = AES;
  hcryp.Init.DataType = CRYP_DATATYPE_32B;
  hcryp.Init.KeySize = CRYP_KEYSIZE_128B;
  hcryp.Init.pKey = aes_key;
  hcryp.Init.Algorithm = CRYP_AES_ECB; // Режим ECB достатній для одного 16-байтного блоку
  HAL_CRYP_Init(&hcryp);
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
