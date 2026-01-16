// Переменные
// Контейнера
let loaderContainer;
let loginContainer;
let confirmContainer;
let lastTab = "";
// Переменные ввода информации
let inputs;
let emailForm, emailInput;
let nicknameForm, nicknameInput;
let firstnameForm, firstnameInput;
let lastnameForm, lastnameInput;
let patronimycForm, patronimycInput;
let passwordForm, passwordInput;
let repeatPasswordForm, repeatPasswordInput;
// Блок подтверждения
let verificationInputs;
// Кнопки и текст подтверждения
let confirmYesButton;
let confirmNoButton;
let confirmText;
// Вывод информации
let notification;
let notificationContent;

// Отдельные функции
// Вспомогательные
// Очистка всех полей ввода
function clear_all_inputs() {
    inputs.forEach(input => {
        input.value = "";
    });
}

// Работа с выбором меню и экрана загрузки
// Загрузка
function open_loading() {
    loginContainer.classList.add("hidden");
    loaderContainer.classList.remove("hidden");
}
// Авторизация
function open_authorization_menu(type) {
    if (lastTab != "authorization") {
        clear_all_inputs();
        lastTab = "authorization";
    }

    loginContainer.classList = `login-container mode-authtorization ${type}`;
    loaderContainer.classList.add("hidden");
    close_notification();
}
// Регистрация
function open_registration_menu() {
    if (lastTab != "registration") {
        clear_all_inputs();
        lastTab = "registration";
    }

    loginContainer.classList = "login-container mode-registration default";
    loaderContainer.classList.add("hidden");
    close_notification();
}
// Подтверждение регистрации
function open_verifing(type) {
    // Очищает ВСЕ поля только если не было открыто меню type до этого
    if (lastTab != type) {
        clear_all_inputs();
        lastTab = type;
    }
    else verificationInputs.forEach((input, index) => { input.value = ""; });

    loginContainer.classList = `login-container mode-verifing ${type}`;
    loaderContainer.classList.add("hidden");
    close_notification();
}

// Отправка уведомления о процессах
let notificationWasClosed = false;
function send_notification(type, text, timeoutSeconds = 7) {
    notificationContent.textContent = text;

    if (type === "error") {
        notification.classList.remove("success");
        notification.classList.add("error");
    }
    else {
        notification.classList.remove("error");
        notification.classList.add("success");
    }

    notification.classList.remove("hidden");

    // Автоматическое скрытие через 3 секунды
    notificationWasClosed = false;
    setTimeout(() => {
        if (!notificationWasClosed) close_notification();
    }, timeoutSeconds * 1000);
}
function close_notification() {
    notificationWasClosed = true;
    notification.classList.add("hidden");
}

// Для отправки запроса на подтверждение
function send_confirm(text, yes_callback, no_callback) {
    confirmText.textContent = text;
    confirmContainer.classList.remove("hidden");
    loaderContainer.classList.add("hidden");
    loginContainer.classList.add("hidden");

    confirmYesButton.onclick = yes_callback;
    confirmNoButton.onclick = no_callback;
}
function close_confirm() {
    confirmContainer.classList.add("hidden")
    loginContainer.classList.remove("hidden");
}

// Работа с куки локально (для registration_state P.S. и authorizationToken)
function save_cookie(name, value, milliseconds, path = "/") {
    const date = new Date();
    date.setTime(date.getTime() + (milliseconds));
    document.cookie = `${name}=${value}; expires=${date.toUTCString()}; path=${path}`;
}
function get_cookie(name) {
    // Получаем все куки как строку
    const cookies = document.cookie;
    // Разбиваем на массив отдельных куки
    const cookieArray = cookies.split(";");
    for (let cookie of cookieArray) {
        cookie = cookie.trim();
        // Находим нужное куки по имени
        if (cookie.startsWith(`${name}=`)) {
            console.log("Куки нашёл.");
            // Если нашли — извлекаем значение (убирая "=" и первую часть)
            const value = cookie.substring(name.length + 1);
            try {
                // Декодируем закодированные спец символы
                return decodeURIComponent(value);
            } catch (e) {
                return value;
            }
        }
    }
    // Если не нашли
    return null;
}
function delete_cookie(name, path = "/") {
    document.cookie = `${name}=; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=${path}`;
}

function build_verify_code() {
    let result = "";
    verificationInputs.forEach((input, index) => {
        result += input.value;
    });
    console.log("Код отправляемый:", result, ". Собран из полей:", verificationInputs.length, verificationInputs);
    return result;
}

// Инициализация переменных и их настройка после загрузке документа
document.addEventListener("DOMContentLoaded", function () {
    // Инициализация переменных
    // Контейнера
    loaderContainer = document.getElementById("loaderContainer");
    loginContainer = document.getElementById("loginContainer");
    confirmContainer = document.getElementById("confirmContainer");
    // Все поля ввода на странице
    inputs = document.querySelectorAll("input");
    // Формы основных полей (с заголовками) на странице
    emailForm = document.getElementById("emailForm");
    nicknameForm = document.getElementById("nicknameForm");
    firstnameForm = document.getElementById("firstnameForm");
    lastnameForm = document.getElementById("lastnameForm");
    patronimycForm = document.getElementById("patronimycForm");
    passwordForm = document.getElementById("passwordForm");
    repeatPasswordForm = document.getElementById("repeatPasswordForm");
    // Основные поля 
    emailInput = document.getElementById("emailInput");
    nicknameInput = document.getElementById("nicknameInput");
    firstnameInput = document.getElementById("firstnameInput");
    lastnameInput = document.getElementById("lastnameInput");
    patronimycInput = document.getElementById("patronimycInput");
    passwordInput = document.getElementById("passwordInput");
    repeatPasswordInput = document.getElementById("repeatPasswordInput");
    // Подтверждение
    verificationInputs = document.querySelectorAll(".code-input");
    // Кнопки и текст подтверждений
    confirmYesButton = document.getElementById("confirmYes");
    confirmNoButton = document.getElementById("confirmNo");
    confirmText = document.getElementById("confirmText");
    // Вывод информации
    notification = document.getElementById("notification");
    notificationContent = document.getElementById("notificationContent");

    // Обработики
    // Ставит коллбэк при вводе кода
    verificationInputs.forEach((input, index) => {
        input.addEventListener("input", function () {
            // Если введено значение (не пустой)
            if (this.value) {
                // Переходим к следующему полю
                const nextIndex = (index + 1) % verificationInputs.length;
                verificationInputs[nextIndex].focus();
            }
        });
    });

    // Заголовки
    document.getElementById("tabAuth").addEventListener("click", function () {
        open_authorization_menu("default");
    })
    document.getElementById("tabRegistration").addEventListener("click", function () {
        open_registration_menu();
    })
    // Кнопка авторизации паролем
    document.getElementById("typeButtonAuthorizationDefault").addEventListener("click", function () {
        open_authorization_menu("default");
    })
    // Кнопка авторизации кодом
    document.getElementById("typeButtonAuthorizationCode").addEventListener("click", function () {
        open_authorization_menu("code");
    })

    // Кнопки входа
    // Собирает все данные с полей и отправляет запрос на отправку кода подтверждения на почту 
    document.getElementById("entryButtonRegistration").addEventListener("click", async function () {
        if (emailInput.value == "" || passwordInput.value == "" || repeatPasswordInput.value == "" || nicknameInput.value == "" ||
            firstnameInput.value == "" || lastnameInput.value == "" || patronimycInput.value == "") {
            send_notification("error", "Поля не должны быть пустыми.");
            return;
        }
        open_loading();
        // Отправляем запрос на регистрацию
        const [status, registration_response] = await post_to_webclient("init/registration", {},
            {
                email: emailInput.value,
                nickname: nicknameInput.value,
                first_name: firstnameInput.value,
                last_name: lastnameInput.value,
                patronimyc: patronimycInput.value,
                password: passwordInput.value,
                repeat_password: repeatPasswordInput.value
            });
        // Обрабатываем ошибки регистрации
        if (!registration_response) {
            open_registration_menu();
            send_notification("error", "Ошибка регистрации.");
            return;
        }
        if (status != 200) {
            open_registration_menu();
            send_notification("error", registration_response["message"]);
            return;
        }
        // В случае успеха, сохраняет state и открывает окно для кода
        save_cookie("registration_state", registration_response["state"], 1000 * 60 * 5);
        open_verifing("registration");
        send_notification("success", registration_response["message"]);
    })
    // Берёт значение из поля почты и отправляет его вместе с запросом на отправку кода подтверждения на авторизированный аккаунт
    document.getElementById("entryButtonAuthorizationCode").addEventListener("click", async function () {
        if (emailInput.value == "") {
            send_notification("error", "Поле не должны быть пустыми.");
            return;
        }
        open_loading();
        // Отправляем запрос на регистрацию
        const [status, auth_response] = await post_to_webclient("init/authorization", {},
            {
                email: emailInput.value,
            });
        // Обрабатываем ошибки 
        if (!auth_response) {
            open_authorization_menu("code");
            send_notification("error", "Ошибка авторизации.");
            return;
        }
        if (status != 200) {
            open_authorization_menu("code");
            send_notification("error", auth_response["message"]);
            return;
        }
        // В случае успеха, сохраняет state и открывает окно для кода
        save_cookie("authorization_state", auth_response["state"], 1000 * 60 * 5);
        open_verifing("authorization");
        send_notification("success", auth_response["message"]);
    })
    // Собирает данные с полей почты и пароля и отправляет запрос на обычную авторизацию 
    document.getElementById("entryButtonAuthorizationDefault").addEventListener("click", async function () {
        if (emailInput.value == "" || passwordInput.value == "") {
            send_notification("error", "Поля не должны быть пустыми.");
            return;
        }
        open_loading();
        const [status, auth_response] = await post_to_webclient("login", { type: "default" },
            {
                email: emailInput.value,
                password: passwordInput.value
            });
        // Обрабатываем ошибки авторизации
        if (!auth_response) {
            open_authorization_menu("default");
            send_notification("error", "Ошибка авторизации.");
            return;
        }
        if (status != 200) {
            open_authorization_menu("default");
            send_notification("error", auth_response["message"]);
            return;
        }
        // Пишет об успехе
        send_notification("success", auth_response["message"]);
        // Входит
        open_webclient_ref("login?target=check_and_confirm");
    })
    // Получает введённый код, state из куки и отправляет запрос на окончательную регистрацию 
    document.getElementById("entryButtonVerifingRegistration").addEventListener("click", async function () {
        let code = build_verify_code();
        // Обработка ситуации, когда открыто подтверждение, но регистрации не было
        const registration_state = get_cookie("registration_state");
        console.log("State для передачи:", registration_state);
        if (!registration_state || registration_state === "") {
            send_notification("error", "Отсутствует процесс регистрации, требующий подтверждения.");
            open_registration_menu();
            return;
        }
        // Обработка не введённого кода
        if (code.length < verificationInputs.length) {
            send_notification("error", "Введите код полностью перед отправкой.")
            return;
        }

        open_loading();
        console.log(registration_state);
        const [status, verify_response] = await post_to_webclient("verify/registration", {},
            {
                code: code,
                state: registration_state,
            });

        // Обрабатываем ошибки регистрации
        if (!verify_response) {
            open_verifing("registration");
            send_notification("error", "Ошибка подтверждения.");
            return;
        }
        if (status != 200) {
            open_verifing("registration");
            send_notification("error", verify_response["message"])
            return;
        }
        // Если регистрация успешна, удаляет токен регистрации и переходим в меню входа
        delete_cookie("registration_state");
        open_authorization_menu("default");
        send_notification("success", verify_response["message"]);
    })
    // Получает введённый код, state из куки и отправляет запрос на окончательную авторизацию кодом
    document.getElementById("entryButtonVerifingAuthorization").addEventListener("click", async function () {
        const code = build_verify_code();
        // Обработка ситуации, когда открыто подтверждение, но авторизации кодом не было
        const authorization_state = get_cookie("authorization_state");
        console.log("State для передачи:", authorization_state);
        if (!authorization_state || authorization_state === "") {
            send_notification("error", "Отсутствует процесс авторизации, требующий подтверждения.");
            open_authorization_menu("default");
            return;
        }
        // Обработка не введённого кода
        if (code.length < verificationInputs.length) {
            send_notification("error", "Введите код полностью перед отправкой.")
            return;
        }

        open_loading();
        const [status, verify_response] = await post_to_webclient("login", { type: "code" },
            {
                state: authorization_state,
                code: code,
            });

        // Обрабатываем ошибки авторизации
        if (!verify_response) {
            open_verifing("authorization");
            send_notification("error", "Ошибка подтверждения.");
            return;
        }
        if (status != 200) {
            open_verifing("authorization");
            send_notification("error", verify_response["message"])
            return;
        }
        // Если регистрация успешна, удаляет токен регистрации и переходим в меню входа
        delete_cookie("authorization_state");
        send_notification("success", verify_response["message"]);
        // Входит
        open_loading();
        open_ref(web_server_url + "/login?target=check_and_confirm");
    })

    // Кнопки сервисов
    document.getElementById("githubAuth").addEventListener("click", async function () {
        open_loading();
        const [status, response] = await post_to_webclient("login", { type: "github" }, {});
        if (response && status == 200) {
            console.log("Переходим на " + response["oauth_redirect_url"]);
            open_ref(response["oauth_redirect_url"]);
        } else {
            open_authorization_menu("default");
            send_notification("error", "Не удалось подключиться к сервису.");
        }
    });
    document.getElementById("yandexAuth").addEventListener("click", async function () {
        open_loading();
        const [status, response] = await post_to_webclient("login", { type: "yandex" }, {});
        if (response && status == 200) {
            console.log("Переходим на " + response["oauth_redirect_url"]);
            open_ref(response["oauth_redirect_url"]);
        } else {
            open_authorization_menu("default");
            send_notification("error", "Не удалось подключиться к сервису.");
        }
    });

    // Ставит ивент на кнопку закрытия уведомления
    document.getElementById("closeNotification").addEventListener("click", function () {
        close_notification();
    });

    // Стартовые функции
    // Если имя не установлено, открываем формы входа
    if (user_name == " <!--Name--> ") {
        // Открывает меню входа, если нет кук на регистрацию. Иначе открывает меню подтверждения регистрации
        const registration_state = get_cookie("registration_state");
        if (!registration_state || registration_state == "") {
            delete_cookie("registration_state");

            // Открывает меню входа, если нет кук на подтверждение входа кодом. 
            const state = get_cookie("authorization_state");
            if (!state || state == "") {
                delete_cookie("authorization_state");
                open_authorization_menu("default");
            }
            else open_verifing("authorization");
        }
        else open_verifing("registration");
    }
    // Иначе открываем вопрос для авто-авторизации пользователя
    else {
        send_confirm("Войти, как " + user_name + " ?",
            async function () { // yes
                open_loading();
                // Переход подтвердит вход, и загрузит личный кабинет
                open_ref(web_server_url + "/login?target=confirm");
            },
            async function () { // no
                open_loading();
                // Переход обнулит сессию, сделает ещё один переход сюда, но без name
                open_ref(web_server_url + "/login?target=cancel");
            }
        );
    }
});
