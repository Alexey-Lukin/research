/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Прошивка вузла Silken Net (Стан Нульового Лагу + TinyML + Кенозис)
  * @processor      : STM32WLE5JC
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
// Флюси для плавки: Підключаємо віртуальну машину mruby
#include <mruby.h>
#include <mruby/irep.h>
#include <mruby/array.h>

// Підключаємо скомпільовану нейромережу TinyML
#include "silken_net_audio_model.h"
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
ADC_HandleTypeDef hadc;
IWDG_HandleTypeDef hiwdg; // Додано: Апаратний сторожовий пес
RNG_HandleTypeDef hrng;
RTC_HandleTypeDef hrtc;
SUBGHZ_HandleTypeDef hsubghz;

/* USER CODE BEGIN PV */

// === 1. ОРГАНИ ЧУТТЯ ТА ПАМ'ЯТЬ ===
volatile uint8_t vibration_detected = 0; // Прапорець переривання від п'єзодиска
uint16_t acoustic_events = 0;          // Відфільтровані мікророзриви (Кавітація)
uint32_t last_wakeup_timestamp = 0;    // Час попереднього пробудження
uint32_t delta_t_seconds = 0;          // Швидкість заряду іоністора (Метаболізм)

// Пейлоад для LoRa (7 байтів: Vcap[2], Temp[1], Acoustic[1], Time[2], Chaos[1])
uint8_t lora_payload[7];

// === 1.5. ПАМ'ЯТЬ TINYML (Свідомість звуку) ===
float audio_buffer[512];       // Буфер для запису звукової хвилі
uint8_t ml_event_id = 0;       // Результат: 0-Тиша, 1-Вітер, 2-Кавітація, 3-Пилка
float ml_confidence = 0.0;     // Рівень впевненості моделі (0.0 - 1.0)

// === 2. РУДА СВІДОМОСТІ (Байт-код mruby) ===
// Скомпільований скрипт Атрактора Лоренца. 
// Цей масив генерується на Mac командою mrbc.
const uint8_t lorenz_bytecode[] = {
  0x52, 0x49, 0x54, 0x45, 0x30, 0x33, 0x30, 0x30, 0x00, 0x00, 
  // ... тут лежать реальні hex-коди вашого Ruby-скрипта ...
  0x00, 0x00, 0x00, 0x01
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC_Init(void);
static void MX_IWDG_Init(void); // Додано: Ініціалізація IWDG
static void MX_RNG_Init(void);
static void MX_RTC_Init(void);
static void MX_SUBGHZ_Init(void);

/* USER CODE BEGIN PFP */
// Псевдо-функції для роботи зі звуком та тривогами
void Record_Audio_Wave(float* buffer, uint16_t length);
void Trigger_Emergency_LoRa_TX(void);
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
  MX_ADC_Init();
  MX_IWDG_Init(); // Ініціалізуємо Сторожового Пса
  MX_RNG_Init();
  MX_RTC_Init();
  MX_SUBGHZ_Init();

  /* USER CODE BEGIN 2 */
  // 1. Відкриваємо доступ до Backup Domain (дозволяємо запис у вічну пам'ять)
  HAL_PWR_EnableBkUpAccess();

  // 2. Відновлюємо пам'ять з RTC (якщо було перезавантаження)
  acoustic_events = (uint16_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0);
  last_wakeup_timestamp = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);

  // Якщо це найперший старт в житті анкера (пам'ять порожня)
  if (last_wakeup_timestamp == 0) {
      last_wakeup_timestamp = HAL_GetTick() / 1000;
  }

  // 3. Калібрування АЦП (Встановлюємо абсолютний фізичний нуль)
  HAL_ADCEx_Calibration_Start(&hadc);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // =========================================================================
    // ФАЗА 0: СИГНАЛ ЖИТТЯ (IWDG)
    // =========================================================================
    // Гладимо Сторожового Пса. Якщо ядро зависне і не виконає цю команду,
    // система автоматично перезавантажиться і відновить дані з RTC.
    HAL_IWDG_Refresh(&hiwdg);

    // =========================================================================
    // ФАЗА 1: ЗБІР ФІЗИЧНИХ ДАНИХ (Нульова ентропія)
    // =========================================================================

    // 1. Метаболізм (Час)
    uint32_t current_time = HAL_GetTick() / 1000; 
    delta_t_seconds = current_time - last_wakeup_timestamp;
    last_wakeup_timestamp = current_time;

    // 2. Внутрішні метрики (Температура та Заряд)
    uint16_t internal_temp = 0;
    uint16_t vcap_voltage = 0;
    
    HAL_ADC_Start(&hadc); 
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
        internal_temp = HAL_ADC_GetValue(&hadc); // Канал температури
    }
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
        vcap_voltage = HAL_ADC_GetValue(&hadc); // Канал VREFINT (іоністор)
    }
    HAL_ADC_Stop(&hadc); // Миттєво вимикаємо АЦП

    // 3. Квантовий Хаос (Зерно для Атрактора)
    uint32_t chaos_seed = 0;
    HAL_RNG_GenerateRandomNumber(&hrng, &chaos_seed);

    // =========================================================================
    // ФАЗА 1.5: TINYML (Шаховий розтин / Фільтрація Свідомості)
    // =========================================================================
    
    // Якщо ядро прокинулось через вібрацію на піні
    if (vibration_detected) {
        // 1. Записуємо звук з АЦП (мікросекундний процес)
        // Record_Audio_Wave(audio_buffer, 512);

        // 2. Запускаємо нейромережу (Whitewash)
        // ml_event_id = Run_Inference(audio_buffer, &ml_confidence);

        // 3. Фільтруємо сміття: довіряємо лише якщо впевненість > 80%
        if (ml_confidence > 0.80) {
            if (ml_event_id == 2) {
                // Це підтверджена кавітація ксилеми!
                acoustic_events++; 
            } else if (ml_event_id == 3) {
                // ТРИВОГА: Незаконна вирубка. Екстрена відправка.
                // Trigger_Emergency_LoRa_TX(); 
            }
        }
        
        // Скидаємо прапорець вібрації
        vibration_detected = 0; 
    }

    // =========================================================================
    // ФАЗА 2: БІТОВЕ ПАКУВАННЯ (Протокол Чистого Транзиту)
    // =========================================================================
    
    // Байти 0-1: Напруга іоністора (mV)
    lora_payload[0] = (uint8_t)(vcap_voltage >> 8);
    lora_payload[1] = (uint8_t)(vcap_voltage & 0xFF);
    
    // Байт 2: Температура (°C)
    lora_payload[2] = (int8_t)__LL_ADC_CALC_TEMPERATURE(3300, internal_temp, LL_ADC_RESOLUTION_12B);
    
    // Байт 3: Акустичні події (Відфільтровані TinyML)
    lora_payload[3] = (uint8_t)(acoustic_events & 0xFF);
    
    // Байти 4-5: Швидкість заряду (Секунди)
    lora_payload[4] = (uint8_t)(delta_t_seconds >> 8);
    lora_payload[5] = (uint8_t)(delta_t_seconds & 0xFF);

    // Обнуляємо лічильник після архівації
    acoustic_events = 0; 

    // =========================================================================
    // ФАЗА 3: ПЛАВКА (Запуск Ruby та Атрактора Лоренца)
    // =========================================================================
    
    // Розпалюємо віртуальну машину
    mrb_state *mrb = mrb_open();

    if (mrb) {
      // Завантажуємо байт-код Атрактора
      mrb_load_irep(mrb, lorenz_bytecode);

      // Формуємо аргументи: [Квантовий Шум, Температура, Акустика]
      mrb_value args[3];
      args[0] = mrb_fixnum_value(chaos_seed);
      args[1] = mrb_fixnum_value(lora_payload[2]); // Температура
      args[2] = mrb_fixnum_value(lora_payload[3]); // Кількість подій (чистий стрес)

      // Викликаємо метод calculate_state
      mrb_value ruby_result = mrb_funcall_argv(mrb, mrb_top_self(mrb), mrb_intern_lit(mrb, "calculate_state"), 3, args);

      // Записуємо результат хаосу у фінальний байт пейлоаду
      lora_payload[6] = (uint8_t)mrb_fixnum(ruby_result);

      // Знищуємо віртуальну машину (звільняємо RAM)
      mrb_close(mrb);
    } else {
      // Якщо VM не запустилася через нестачу пам'яті
      lora_payload[6] = 0xFF; 
    }

    // =========================================================================
    // ФАЗА 4: ПЕРЕДАЧА ДАНИХ (LoRaWAN)
    // =========================================================================
    // Тут буде функція передачі payload через стек LoRaWAN.
    // Наприклад: LORA_SendPayload(lora_payload, 7);

    // =========================================================================
    // ФАЗА 5: КЕНОЗИС (Абсолютний сон та збереження)
    // =========================================================================
    // Ховаємо життєво важливі дані в RTC перед вимкненням оперативної пам'яті
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, acoustic_events);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, last_wakeup_timestamp);

    // Listen for the whisper. Відключаємо ядро і чекаємо.
    HAL_SuspendTick();
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
    HAL_ResumeTick();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

// =========================================================================
// АПАРАТНИЙ РЕФЛЕКС (Голос Дерева)
// =========================================================================
// Викликається апаратно, коли п'єзодиск генерує імпульс (PA0).
// Тепер він не додає подію сліпо, а лише будить ядро для аналізу TinyML.
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == GPIO_PIN_0) 
  {
    vibration_detected = 1; // Піднімаємо прапорець для Фази 1.5
  }
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
