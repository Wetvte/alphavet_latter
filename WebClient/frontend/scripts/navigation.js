document.addEventListener('DOMContentLoaded', async () => {
    // Назад
    const backButton = document.getElementById("backButton");
    if (backButton) backButton.addEventListener("click", () => {
        console.log("Back");
        history.back();
    });

    // Кабинет
    const cabinetButton = document.getElementById("cabinetButton");
    if (cabinetButton) cabinetButton.addEventListener('click', () => {
        open_webclient_ref("cabinet");
    });

    // Главная
    const primaryButton = document.getElementById('primaryButton');
    if (primaryButton) primaryButton.addEventListener('click', () => {
        open_webclient_ref("primary");
    });

});