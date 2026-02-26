# frozen_string_literal: true

class BurnCarbonTokensWorker
  include Sidekiq::Job

  # Використовуємо ту саму чергу для повільних блокчейн-операцій
  sidekiq_options queue: 'web3', retry: 5

  def perform(organization_id, naas_contract_id)
    Rails.logger.warn "🔥 [D-MRV Slashing] Ініціація протоколу спалювання для Контракту ##{naas_contract_id}"
    
    BlockchainBurningService.call(organization_id, naas_contract_id)
    
  rescue StandardError => e
    Rails.logger.error "🚨 [D-MRV Slashing] Помилка спалювання: #{e.message}"
    raise e
  end
end
