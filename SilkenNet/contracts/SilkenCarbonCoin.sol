// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

// Імпортуємо перевірені часом і мільярдами доларів стандарти від OpenZeppelin
import "@openzeppelin/contracts/token/ERC20/ERC20.sol";
import "@openzeppelin/contracts/access/Ownable.sol";

/**
 * @title Silken Carbon Coin (SCC)
 * @dev ERC20 Токен для D-MRV системи Silken Net. 
 * Мінтити токени може ТІЛЬКИ авторизований Оракул (наш Rails сервер).
 */
contract SilkenCarbonCoin is ERC20, Ownable {

    // Спеціальна подія для аудиту. Коли Rails ініціює мінтинг, 
    // блокчейн назавжди записує DID дерева та кількість вуглецю.
    event CarbonMinted(address indexed investor, uint256 amount, string treeDid);

    /**
     * @dev Конструктор. Встановлює назву токена, символ та адресу Оракула.
     * @param initialOracle Адреса гаманця нашого Rails-сервера
     */
    constructor(address initialOracle) 
        ERC20("Silken Carbon Coin", "SCC") 
        Ownable(initialOracle) 
    {}

    /**
     * @dev Головна функція мінтингу. 
     * Модифікатор `onlyOwner` гарантує, що якщо будь-хто інший 
     * (навіть творець контракту) спробує її викликати, транзакція впаде з помилкою.
     * * @param to Адреса гаманця інвестора (Organization.crypto_public_address)
     * @param amount Кількість токенів (у wei)
     * @param treeDid Унікальний ідентифікатор дерева, що згенерувало бали
     */
    function mint(address to, uint256 amount, string calldata treeDid) public onlyOwner {
        // Випускаємо токени на адресу інвестора
        _mint(to, amount);
        
        // Випромінюємо подію для публічного D-MRV аудиту
        emit CarbonMinted(to, amount, treeDid);
    }

    /**
     * @dev Функція для спалювання токенів (Carbon Offsetting).
     * Коли компанія (наприклад, Microsoft) хоче офіційно "погасити" свій вуглецевий слід,
     * вона спалює ці токени, назавжди виводячи їх з обігу.
     */
    function burn(uint256 amount) public {
        _burn(_msgSender(), amount);
    }
}
