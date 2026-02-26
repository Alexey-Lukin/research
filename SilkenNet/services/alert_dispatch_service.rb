# Цей сервіс викликається наприкінці TelemetryUnpackerService
class AlertDispatchService
  def self.analyze_and_trigger!(telemetry_log)
    tree = telemetry_log.tree

    # 1. Вандалізм (Tamper Detection) - Найвищий пріоритет
    if telemetry_log.tamper_detected?
      EwsAlert.create!(
        cluster: tree.cluster,
        tree: tree,
        severity: :critical,
        alert_type: :vandalism_breach,
        message: "КРИТИЧНО: Зафіксовано відкриття корпусу S-NET! Дерево DID: #{tree.did}"
      )
      # TODO: Виклик API Twilio для SMS / Дзвінка ліснику
    end

    # 2. Пожежа / Пилкова активність (Аномалія Атрактора)
    if telemetry_log.bio_status_anomaly?
      EwsAlert.create!(
        cluster: tree.cluster,
        tree: tree,
        severity: :high,
        alert_type: :biological_threat,
        message: "АНОМАЛІЯ: Критичний стрес ксилеми (Z: #{telemetry_log.growth_points}). Можлива пожежа або пилка."
      )
    end
  end
end
