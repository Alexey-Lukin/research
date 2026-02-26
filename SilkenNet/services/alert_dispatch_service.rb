# frozen_string_literal: true

class AlertDispatchService
  # Фізичні пороги для тригерів (можуть бути винесені в налаштування Cluster)
  SEISMIC_THRESHOLD_MV = 1500
  FIRE_TEMP_THRESHOLD_C = 60.0

  def self.analyze_and_trigger!(telemetry_log)
    tree = telemetry_log.tree
    cluster = tree.cluster

    # 1. ВАНДАЛІЗМ (Tamper Detection - Найвищий пріоритет)
    # Спрацьовує, якщо мікроконтролер фізично відкрили або зірвали з анкера.
    if telemetry_log.tamper_detected?
      create_and_dispatch_alert!(
        cluster: cluster,
        tree: tree,
        severity: :critical,
        alert_type: :vandalism_breach,
        message: "КРИТИЧНО: Зафіксовано відкриття титанового корпусу S-NET! Можливе викрадення. Дерево DID: #{tree.did}"
      )
    end

    # 2. ПОЖЕЖА або РОБОТА ПИЛКОЮ (Екстремальна температура або критичний стрес ксилеми)
    if telemetry_log.temperature_c >= FIRE_TEMP_THRESHOLD_C || telemetry_log.bio_status_anomaly?
      create_and_dispatch_alert!(
        cluster: cluster,
        tree: tree,
        severity: :critical,
        alert_type: :fire_detected, # Використовуємо тип для пожежі/знищення
        message: "КАТАСТРОФА: Термістор фіксує #{telemetry_log.temperature_c}°C або критичний розрив ксилеми (Аномалія Z). Ризик пожежі/вирубки!"
      )
    end

    # 3. ПОСУХА (Тривалий гідрологічний стресс)
    if telemetry_log.bio_status_stress?
      create_and_dispatch_alert!(
        cluster: cluster,
        tree: tree,
        severity: :high,
        alert_type: :severe_drought,
        message: "ПОПЕРЕДЖЕННЯ: Дерево у стані глибокого гідрологічного стресу. Атрактор Лоренца вийшов за межі гомеостазу."
      )
    end

    # 4. ЗЕМЛЕТРУС (Сейсмічний метаматеріал)
    # Коріння вловлює п'єзоелектричний резонанс кристалічного щита
    if telemetry_log.piezo_voltage_mv && telemetry_log.piezo_voltage_mv > SEISMIC_THRESHOLD_MV
      create_and_dispatch_alert!(
        cluster: cluster,
        tree: tree,
        severity: :critical,
        alert_type: :seismic_anomaly,
        message: "СЕЙСМІКА: Аномальний п'єзо-резонанс (#{telemetry_log.piezo_voltage_mv} мВ). Можливий тектонічний зсув."
      )
    end

    # 5. ШКІДНИКИ (Короїд - Edge AI)
    # Якщо нейромережа TinyML класифікувала специфічний акустичний патерн
    # (Припустимо, алгоритм видає велику кількість акустичних подій на тлі стресу)
    if telemetry_log.acoustic_events > 50 && telemetry_log.bio_status_stress?
      create_and_dispatch_alert!(
        cluster: cluster,
        tree: tree,
        severity: :high,
        alert_type: :insect_epidemic,
        message: "БІО-ЗАГРОЗА: Периферійний ШІ зафіксував акустичну емісію, характерну для личинок короїда."
      )
    end
  end

  private_class_method def self.create_and_dispatch_alert!(cluster:, tree:, severity:, alert_type:, message:)
    # 1. Записуємо загрозу в базу даних
    alert = EwsAlert.create!(
      cluster: cluster,
      tree: tree,
      severity: severity,
      alert_type: alert_type,
      message: message
    )

    Rails.logger.warn "🚨 [ALERT DISPATCHER] Згенеровано тривогу: #{alert_type} для Дерева #{tree.did}"

    # 2. ЗАМКНЕНИЙ ЦИКЛ: Миттєво передаємо тривогу в Центр Прийняття Рішень
    # Цей сервіс знайде найближчі клапани, сирени або маяки та активує їх
    EmergencyResponseService.call(alert)

    # 3. Сповіщення людей (Відправка SMS / Push повідомлень ліснику та інвестору)
    notify_stakeholders(alert)
  end

  private_class_method def self.notify_stakeholders(alert)
    # Інтеграція з каналами зв'язку (Twilio, ActionCable для Web-дашборда, Firebase Push)
    # Викликаємо асинхронний воркер, щоб не блокувати процес розпакування телеметрії
    
    # SmsNotificationWorker.perform_async(alert.id)
    # ActionCable.server.broadcast("cluster_#{alert.cluster_id}_alerts", { alert: alert.as_json })
  end
end
