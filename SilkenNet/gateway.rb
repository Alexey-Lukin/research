# frozen_string_literal: true

class Gateway < ApplicationRecord
  # Королева може належати до кластера (але може бути і тестовою, без лісу)
  belongs_to :cluster, optional: true
  
  # Всі пакети, які пройшли через цю Королеву
  has_many :telemetry_logs, foreign_key: :queen_uid, primary_key: :uid, dependent: :nullify

  # uid - це унікальний ідентифікатор модему/мікроконтролера
  validates :uid, presence: true, uniqueness: true

  # СКОУПИ для дашборду
  scope :online, -> { where('last_seen_at >= ?', 1.hour.ago) }
  scope :offline, -> { where('last_seen_at < ? OR last_seen_at IS NULL', 1.hour.ago) }

  # Метод для оновлення пульсу Королеви
  def mark_seen!
    touch(:last_seen_at)
  end
end
