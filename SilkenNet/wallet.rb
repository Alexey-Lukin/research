# frozen_string_literal: true

class Wallet < ApplicationRecord
  belongs_to :tree
  
  # Історія виведення коштів на зовнішній блокчейн
  has_many :blockchain_transactions, dependent: :destroy

  validates :balance, presence: true, numericality: { greater_than_or_equal_to: 0 }

  # Безпечне нарахування токенів. 
  # Використовуємо атомарний SQL-запит (UPDATE wallets SET balance = balance + X) 
  # для уникнення стану гонки (Race Condition), якщо дані прийдуть одночасно.
  def credit!(points)
    increment!(:balance, points)
  end

  # Підготовка до мінтингу на блокчейні.
  # Блокуємо рядок у базі (Pessimistic Locking), списуємо баланс і створюємо транзакцію.
  def lock_and_mint!(amount, type = :carbon_coin)
    transaction do
      lock!
      raise ActiveRecord::RecordInvalid, "Недостатньо балів на балансі" if balance < amount
      
      decrement!(:balance, amount)
      blockchain_transactions.create!(
        amount: amount, 
        token_type: type, 
        status: :pending
      )
    end
  end
end
