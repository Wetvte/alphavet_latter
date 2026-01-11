document.addEventListener('DOMContentLoaded', () => {
    // Находим все input
    const modifiedInputs = document.querySelectorAll('input');
    // Для каждого поля навешиваем обработчик события 'input'
    modifiedInputs.forEach(input => {
        input.addEventListener('input', () => {
            // Удаляем переносы строк, табуляции, множественные пробелы и прочие пробельные символы по краям
            const cleanedValue = input.value
                .replace(/[\r\n\t\f\v]+/g, '')  // удаляем \r, \n, \t
                .replace(/\s+/g, ' ');              // заменяем любые последовательности пробелов на один пробел
            // Обновляем значение поля
            input.value = cleanedValue;
        });
        input.addEventListener('change', () => {
            // Удаляем переносы строк, табуляции, множественные пробелы и прочие пробельные символы по краям
            const cleanedValue = input.value.trim(); // убираем пробелы в начале и конце
            // Обновляем значение поля
            input.value = cleanedValue;
        });
    });
});
