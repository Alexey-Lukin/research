# frozen_string_literal: true

require 'eth'

class BlockchainBurningService
  # Зверніть увагу: ми використовуємо нову функцію `slash` замість `mint`
  CONTRACT_ABI = '[{"inputs":[{"internalType":"address","name":"investor","type":"address"},{"internalType":"uint256","name":"amount","type":"uint256"}],"name":"slash","outputs":[],"stateMutability":"nonpayable","type":"function"}]'

  def self.call(organization_id, naas_contract_id)
    new(organization_id, naas_contract_id).call
  end

  def initialize(organization_id, naas_contract_id)
    @organization = Organization.find(organization_id)
    @naas_contract = NaasContract.find(naas_contract_id)
    @cluster = @naas_contract.cluster
  end

  def call
    # 1. Знаходимо всі успішно намічені токени для цього кластера
    # Шукаємо транзакції, гаманці яких належать деревам із цього кластера
    total_minted_amount = BlockchainTransaction
                          .joins(wallet: :tree)
                          .where(trees: { cluster_id: @cluster.id })
                          .where(status: :confirmed)
                          .sum(:amount)

    return if total_minted_amount.zero?

    investor_address = @organization.crypto_public_address
    
    # 2. Підготовка до Web3
    client = Eth::Client.create(ENV.fetch('ALCHEMY_POLYGON_RPC_URL'))
    oracle_key = Eth::Key.new(priv: ENV.fetch('ORACLE_PRIVATE_KEY'))
    contract_address = ENV.fetch('CARBON_COIN_CONTRACT_ADDRESS')
    contract = Eth::Contract.from_abi(name: "SilkenCarbonCoin", address: contract_address, abi: CONTRACT_ABI)

    amount_in_wei = total_minted_amount * (10**18)

    begin
      Rails.logger.warn "🔥 [Web3] Спалювання #{total_minted_amount} SCC з гаманця #{investor_address}..."

      # 3. Виклик функції slash (каральне спалювання)
      tx_hash = client.transact_and_wait(
        contract, 
        "slash", 
        investor_address, 
        amount_in_wei, 
        sender_key: oracle_key
      )
      
      # 4. Записуємо цю подію в базу як нову транзакцію, щоб інвестор бачив це в історії
      BlockchainTransaction.create!(
        wallet_id: @cluster.trees.first.wallet.id, # Прив'язуємо до одного з дерев кластера
        amount: total_minted_amount,
        token_type: :carbon_coin,
        status: :confirmed,
        tx_hash: tx_hash,
        notes: "SLASHING: Контракт ##{@naas_contract.id} розірвано. Ліс знищено."
      )

      Rails.logger.info "✅ [Web3] Токени успішно спалені! Хеш: #{tx_hash}"

    rescue StandardError => e
      # Якщо транзакція впала (наприклад, інвестор вже продав токени на біржі - 
      # це окремий юридичний кейс, але технічно ми ловимо помилку тут)
      Rails.logger.error "🛑 [Web3] Slashing Failed: #{e.message}"
      raise e
    end
  end
end
