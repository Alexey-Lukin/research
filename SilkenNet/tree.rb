# frozen_string_literal: true

class Tree < ApplicationRecord
  belongs_to :cluster, optional: true
  
  # Гаманець знищується разом з деревом, якщо воно вмирає/спилюється
  has_one :wallet, dependent: :destroy
  
  # Історія пульсу знищується (або можна залишити nullify для архіву)
  has_many :telemetry_logs, dependent: :destroy

  # did - це 32-бітний хеш, згенерований в main.c з UID STM32 та шуму кристала
  validates :did, presence: true, uniqueness: true

  # Додаткові поля в БД:
  # - species: string (Порода: "Дуб", "Сосна")
  # - latitude: decimal (GPS)
  # - longitude: decimal (GPS)
  # - planted_at: datetime (Дата встановлення анкера)

  # Автоматично створюємо гаманець при реєстрації нового дерева
  after_create :build_default_wallet

  private

  def build_default_wallet
    create_wallet!(balance: 0)
  end
end
