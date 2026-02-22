# frozen_string_literal: true

module SilkenNet
  # 1. Математичне ядро (Теорія Хаосу)
  class Attractor
    # Класичні константи Лоренца
    BASE_SIGMA = 10.0
    BASE_RHO   = 28.0
    BASE_BETA  = 2.666 # 8.0 / 3.0
    
    # Крок інтегрування
    DT = 0.01

    def self.calculate_z_axis(seed, temp, acoustic)
      x = ((seed % 1000) / 500.0) - 1.0
      y = (((seed >> 4) % 1000) / 500.0) - 1.0
      z = (((seed >> 8) % 1000) / 500.0) - 1.0

      local_sigma = BASE_SIGMA + (acoustic * 0.1)
      local_rho = BASE_RHO + (temp * 0.2)

      10.times do
        dx = local_sigma * (y - x)
        dy = x * (local_rho - z) - y
        dz = (x * y) - (BASE_BETA * z)

        x += dx * DT
        y += dy * DT
        z += dz * DT
      end

      # Повертаємо чисту інтенсивність конвекції (руху соку)
      return z
    end
  end

  # 2. Логіка прийняття рішень (Біо-Контракт)
  class BioContract
    # Межі детермінованого хаосу здорового дерева
    CRITICAL_Z_MIN = 2.0  # Падіння нижче = втрата тургору / зупинка сокоруху
    CRITICAL_Z_MAX = 45.0 # Стрибок вище = аномальний стрес / втручання

    def self.evaluate_and_pack(seed, temp, acoustic)
      z_val = Attractor.calculate_z_axis(seed, temp, acoustic)
      
      # Визначаємо статус життєздатності (2 біти)
      # 00 (0) - Гомеостаз (Здоровий Хаос)
      # 01 (1) - Сигнал раннього попередження: Посуха (Z впало)
      # 10 (2) - Аномалія: Критичний стрес або зовнішній вплив (Z зашкалює)
      status = 0
      if z_val < CRITICAL_Z_MIN
        status = 1 
      elsif z_val > CRITICAL_Z_MAX
        status = 2
      end

      # Масштабуємо Z у 6-бітне число (від 0 до 63)
      z_int = (z_val * 1.5).to_i
      z_int = 63 if z_int > 63
      z_int = 0  if z_int < 0

      # Пакуємо все в один байт: [ Статус (2 біти) | Значення Z (6 бітів) ]
      # Цей єдиний байт - це все, що полетить через LoRaWAN.
      payload_byte = (status << 6) | z_int
      
      return payload_byte
    end
  end
end

# Точка входу для C-коду. 
# C-ядро викликає саме цю глобальну функцію.
def calculate_state(seed, temp, acoustic)
  SilkenNet::BioContract.evaluate_and_pack(seed, temp, acoustic)
end
