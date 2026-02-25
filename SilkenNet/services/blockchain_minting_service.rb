# frozen_string_literal: true

require 'eth'

class BlockchainMintingService
  # Нам не потрібен повний ABI контракту, достатньо лише функції mint, 
  # щоб Rails знав, як правильно закодувати параметри (ABI Encoding).
  CONTRACT_ABI = '[{"inputs":[{"internalType":"address","name":"to","type":"address"},{"internalType":"uint256","name":"amount","type":"uint256"},{"internalType":"string","name":"treeDid","type":"string"}],"name":"mint","outputs":[],"stateMutability":"nonpayable","type":"function"}]'

  def self.call(blockchain_transaction_id)
    new(blockchain_transaction_id).call
  end

  def initialize(blockchain_transaction_id)
    @transaction = BlockchainTransaction.find(blockchain_transaction_id)
    @wallet = @transaction.wallet
    
    # Знаходимо дерево (Солдата), чиї зусилля ми зараз токенізуємо
    @tree = @wallet.tree 
  end

  def call
    # Захист від подвійного мінтингу
    return unless @transaction.status_pending?

    # 1. Підключення до ноди (Polygon Mainnet або Amoy Testnet через Alchemy)
    # Alchemy забезпечує стабільний RPC-зв'язок без підняття власної ноди
    client = Eth::Client.create(ENV.fetch('ALCHEMY_POLYGON_RPC_URL'))
    
    # 2. Відновлення гаманця Оракула з приватного ключа сервера
    # УВАГА: Цей ключ має бути лише в ENV-змінних або у Vault, ніколи в коді!
    oracle_key = Eth::Key.new(priv: ENV.fetch('ORACLE_PRIVATE_KEY'))
    
    # 3. Ініціалізація контракту
    contract_address = ENV.fetch('CARBON_COIN_CONTRACT_ADDRESS')
    contract = Eth::Contract.from_abi(name: "SilkenCarbonCoin", address: contract_address, abi: CONTRACT_ABI)

    # 4. Підготовка даних
    # У блокчейні немає дробів. 1 токен = 1 * 10^18 wei.
    amount_in_wei = @transaction.amount * (10**18)
    investor_address = @wallet.crypto_public_address
    tree_did = @tree.did

    begin
      Rails.logger.info "⏳ [Web3] Ініціація мінтингу #{@transaction.amount} SCC для дерева #{tree_did}..."

      # 5. Магія: формування, підпис (ECDSA) та відправка транзакції
      # transact_and_wait блокує потік, поки нода не підтвердить, що транзакція включена в блок
      tx_hash = client.transact_and_wait(
        contract, 
        "mint", 
        investor_address, 
        amount_in_wei, 
        tree_did, 
        sender_key: oracle_key
      )
      
      # 6. Успіх: записуємо хеш назавжди
      @transaction.confirm_minting!(tx_hash)
      Rails.logger.info "✅ [Web3] Успішний мінтинг! Хеш: #{tx_hash}"

    rescue StandardError => e
      # 7. Обробка аварій (Немає грошей на газ, Alchemy впав, RPC відхилив)
      Rails.logger.error "🛑 [Web3] Помилка мінтингу: #{e.message}. Виконуємо Rollback."
      
      ActiveRecord::Base.transaction do
        # Позначаємо транзакцію як провальну
        @transaction.update!(status: :failed)
        
        # Повертаємо чесно зароблені бали назад на баланс дерева, 
        # щоб наступна спроба (наприклад, завтра) їх забрала
        @wallet.increment!(:balance, @transaction.amount)
      end
      
      # Прокидаємо помилку вище (щоб Sidekiq знав, що задача впала, або Sentry зловив алерт)
      raise e
    end
  end
end
