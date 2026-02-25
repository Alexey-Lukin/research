# frozen_string_literal: true

class NaasContract < ApplicationRecord
  # Інвестор спонсорує цілий кластер дерев
  belongs_to :cluster
  
  # (В майбутньому тут може бути belongs_to :organization для інвестора)

  enum :status, {
    draft: 0,     # Контракт готується
    active: 1,    # Спонсорування йде, дерева здорові
    fulfilled: 2, # Термін дії закінчився успішно
    breached: 3   # Контракт розірвано (наприклад, ліс згорів або вирубаний)
  }, prefix: true

  validates :sponsor_name, :total_funding, :start_date, :end_date, presence: true

  # Логіка D-MRV: Контракт автоматично скасовується, якщо кластер гине
  def check_cluster_health!
    return unless status_active?

    # Якщо більше 20% дерев у кластері мають критичні аномалії — контракт порушено
    anomalous_trees = cluster.trees.joins(:telemetry_logs).where(telemetry_logs: { bio_status: :anomaly }).distinct.count
    
    if anomalous_trees > (cluster.trees.count * 0.20)
      update!(status: :breached)
      # TODO: Надіслати сповіщення інвестору та припинити мінтинг токенів
    end
  end
end
