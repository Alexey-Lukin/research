# frozen_string_literal: true

class TelemetryLog < ApplicationRecord
  # Дерево, яке згенерувало цей пакет
  belongs_to :tree
  
  # Королева, яка зловила цей пакет (опціонально, бо пакети можуть губитися або йти через різних Королев)
  belongs_to :gateway, foreign_key: :queen_uid, primary_key: :uid, optional: true

  # Маппінг 2-бітного статусу з нашого Ruby-контракту на мікроконтролері
  # (Ті самі статуси з bio_contract.rb: 0 - Гомеостаз, 1 - Посуха, 2 - Аномалія)
  enum :bio_status, {
    homeostasis: 0, # Здоровий Хаос
    stress: 1,      # Сигнал раннього попередження (Посуха)
    anomaly: 2      # Критичний стрес / Пилка
  }, prefix: true

  # Базові фізичні метрики, які ми гарантовано дістаємо з розпакувальника
  validates :voltage_mv, :temperature_c, :acoustic_events, :metabolism_s, :growth_points, presence: true

  # СКОУПИ ДЛЯ ДАШБОРДІВ
  # Отримати найсвіжіші дані
  scope :recent, -> { order(created_at: :desc) }
  
  # Отримати тільки проблемні пакети (для аналітики)
  scope :anomalies, -> { where(bio_status: [:stress, :anomaly]).or(where('acoustic_events > ?', 0)) }
  
  # Фільтр за часовим проміжком (корисно для відмальовки Атрактора Лоренца на фронтенді)
  scope :in_timeframe, ->(start_time, end_time) { where(created_at: start_time..end_time) }
end
