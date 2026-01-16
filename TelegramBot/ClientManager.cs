using System;
using System.Linq;
using System.Threading;
using System.Collections.Generic;
using Telegram.Bot;
using Telegram.Bot.Types;
using Telegram.Bot.Polling;
using Telegram.Bot.Types.Enums;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace TelegramBot {
    public class ClientManager
    {
        private class ClientUpdateHandler : IUpdateHandler
        {
            private readonly ApiClient controllerClient;

            public ClientUpdateHandler()
            {
                // Создание клиента контроллера (ов)
                controllerClient = new ApiClient(ConfigLoader.Get("SERVER_URL"));
            }

            // Внутренний класс для настройки телеграм клиента
            public async Task HandleUpdateAsync(ITelegramBotClient bot, Update update, CancellationToken token)
            {
                string text = update.Message.Text ?? "/unknown";
                long chatId = update.Message.Chat.Id;
                Console.WriteLine($"Получено сообщение {text} из чата с айди {chatId}");

                if (text[0] != '/') {
                    text = "/unknown";
                }

                Console.WriteLine("Найдено объявление команды в сообщении.");
                // Парсим текст в команды
                List<Command> commands = ClientManager.Command.Parse(text);
                Console.WriteLine("Интерпретировано, как команды: " + string.Join(", ", commands));
                // Обрабатывает каждую команду
                foreach (Command command in commands) {
                    // Создаём тело запроса
                    JObject body = new JObject();
                    body["chat_id"] = chatId;
                    // Обработка команды
                    Console.WriteLine("Обработка команды: " + command.Action);
                    int i, j;
                    bool success = false;
                    switch (command.Action)
                    {
                        // Команды Общей информации
                        case "/start":
                            await controllerClient.PostAsync("start", body);
                            success = true;
                            break;
                        case "/help":
                            string filter = "";
                            if (command.ConfigurationsCount() > 0)
                            {
                                for (i = 0; i < command.ConfigurationsCount(); i++)
                                {
                                    if (command.GetConfiguration(i).Operation == "-all")
                                    {
                                        filter = "-all";
                                        break;
                                    }
                                    if (filter == "") filter += " ";
                                    filter += command.GetConfiguration(i).Operation;
                                }
                            }
                            else filter = "-commands";
                            body["filter"] = filter;
                            await controllerClient.PostAsync("help", body);
                            success = true;
                            break;
                        // Команды Авторизации
                        case "/reg":
                            if (command.ConfigurationsCount() >= 1) switch (command.GetConfiguration(0).Operation)
                                {
                                    case "-init":
                                        if (command.GetConfiguration(0).ParametresCount() >= 7)
                                        {
                                            body["email"] = command.GetConfiguration(0).GetParametr(0);
                                            body["nickname"] = command.GetConfiguration(0).GetParametr(1);
                                            body["first_name"] = command.GetConfiguration(0).GetParametr(2);
                                            body["last_name"] = command.GetConfiguration(0).GetParametr(3);
                                            body["patronimyc"] = command.GetConfiguration(0).GetParametr(4);
                                            body["password"] = command.GetConfiguration(0).GetParametr(5);
                                            body["repeat_password"] = command.GetConfiguration(0).GetParametr(6);
                                        }
                                        await controllerClient.PostAsync("init/registration", body);
                                        success = true;
                                        break;
                                    case "-verify":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["code"] = command.GetConfiguration(0).GetParametr(0);
                                        }
                                        await controllerClient.PostAsync("verify/registration", body);
                                        success = true;
                                        break;
                                }
                            break;
                        case "/login":
                            if (command.ConfigurationsCount() >= 1) switch (command.GetConfiguration(0).Operation)
                                {
                                    case "-default":
                                        if (command.GetConfiguration(0).ParametresCount() >= 2)
                                        {
                                            body["email"] = command.GetConfiguration(0).GetParametr(0);
                                            body["password"] = command.GetConfiguration(0).GetParametr(1);
                                        }
                                        await controllerClient.PostAsync("login?type=default", body);
                                        success = true;
                                        break;
                                    case "-code":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["email"] = command.GetConfiguration(0).GetParametr(0);
                                            success = true;
                                        }
                                        if (command.GetConfiguration(0).ParametresCount() >= 2)
                                        {
                                            body["code"] = command.GetConfiguration(0).GetParametr(1);
                                            await controllerClient.PostAsync("login?type=code", body);
                                        }
                                        else await controllerClient.PostAsync("init/authorization", body);
                                        break;
                                    case "-yandex":
                                        await controllerClient.PostAsync("login?type=yandex", body);
                                        success = true;
                                        break;
                                    case "-github":
                                        await controllerClient.PostAsync("login?type=github", body);
                                        success = true;
                                        break;
                                }
                            break;
                        case "/logout":
                            body["all"] = command.ConfigurationsCount() >= 1 && command.GetConfiguration(0).Operation == "-all";
                            await controllerClient.PostAsync("logout", body);
                            success = true;
                            break;
                        // Команды Информации
                        case "/users":
                            await controllerClient.PostAsync("users", body);
                            success = true;
                            break;
                        case "/user":
                            // Работает только с конфигурацией
                            if (command.ConfigurationsCount() >= 1)
                            {
                                // Для каждой конфигурации
                                for (i = 0; i < command.ConfigurationsCount(); i++)
                                {
                                    // У конфигурации должен быть минимум один параметр (айди)
                                    if (command.GetConfiguration(i).ParametresCount() >= 1)
                                    {
                                        body["user_id"] = command.GetConfiguration(i).GetParametr(0);
                                        // Если без доп. параметров
                                        if (command.GetConfiguration(i).ParametresCount() == 1)
                                        {
                                            switch (command.GetConfiguration(i).Operation)
                                            {
                                                case "-email": body["filter"] = "email"; break;
                                                case "-nickname": body["filter"] = "nickname"; break;
                                                case "-fullname": body["filter"] = "fullname"; break;
                                                case "-roles": body["filter"] = "roles"; break;
                                                case "-status": body["filter"] = "status"; break;
                                                case "-profile": body["filter"] = "email nickname fullname roles status"; break;
                                                case "-disciplines": body["filter"] = "disciplines"; break;
                                                case "-grades": body["filter"] = "tries"; break;
                                                case "-tests": body["filter"] = "tests"; break;
                                            }
                                            // Если операция соответствует существующим
                                            if (body.ContainsKey("filter"))
                                            {
                                                await controllerClient.PostAsync("user/info/read", body);
                                                success = true;
                                            }
                                        }
                                        // Если с доп параметрами 
                                        else
                                        {
                                            switch (command.GetConfiguration(i).Operation)
                                            {
                                                case "-email": body["operation"] = "email"; break;
                                                case "-nickname": body["operation"] = "nickname"; break;
                                                case "-fullname": body["operation"] = "fullname"; break;
                                                case "-roles": body["operation"] = "roles"; break;
                                                case "-status": body["operation"] = "status"; break;
                                            }
                                            // Если операция соответствует существующим
                                            if (body.ContainsKey("operation") &&
                                                // Если операция - ФИО, проверяет на кол-во параметров (Ф+И+О+АЙДИ)
                                                (body["operation"].ToString() != "-fullname" || command.GetConfiguration(i).ParametresCount() >= 4))
                                            {
                                                JArray values = new JArray();
                                                // Вбивает все переданные параметры, пропуская айди
                                                for (j = 1; j < command.GetConfiguration(i).ParametresCount(); j++)
                                                {
                                                    values.Add(command.GetConfiguration(i).GetParametr(j));
                                                }
                                                body["values"] = values;
                                                await controllerClient.PostAsync("user/info/write", body);
                                                success = true;
                                            }
                                        }
                                    }
                                }
                            }
                            break;
                        case "/me":
                            // Работает только с конфигурацией
                            if (command.ConfigurationsCount() >= 1)
                            {
                                // Для каждой конфигурации
                                for (i = 0; i < command.ConfigurationsCount(); i++)
                                {
                                    // Если без параметров
                                    if (command.GetConfiguration(i).ParametresCount() == 0)
                                    {
                                        switch (command.GetConfiguration(i).Operation)
                                        {
                                            case "-email": body["filter"] = "email"; break;
                                            case "-nickname": body["filter"] = "nickname"; break;
                                            case "-fullname": body["filter"] = "fullname"; break;
                                            case "-roles": body["filter"] = "roles"; break;
                                            case "-status": body["filter"] = "status"; break;
                                            case "-profile": body["filter"] = "email nickname fullname roles status"; break;
                                            case "-disciplines": body["filter"] = "disciplines"; break;
                                            case "-grades": body["filter"] = "tries"; break;
                                            case "-tests": body["filter"] = "tests"; break;
                                        }
                                        // Если операция соответствует существующим
                                        if (body.ContainsKey("filter"))
                                        {
                                            await controllerClient.PostAsync("user/info/read", body);
                                            success = true;
                                        }
                                    }
                                    // Если с параметрами 
                                    else
                                    {
                                        switch (command.GetConfiguration(i).Operation)
                                        {
                                            case "-email": body["operation"] = "email"; break;
                                            case "-nickname": body["operation"] = "nickname"; break;
                                            case "-fullname": body["operation"] = "fullname"; break;
                                            case "-roles": body["operation"] = "roles"; break;
                                            case "-status": body["operation"] = "status"; break;
                                        }
                                        // Если операция соответствует существующим
                                        if (body.ContainsKey("operation") &&
                                            // Если операция - ФИО, проверяет на кол-во параметров (Ф+И+О)
                                            (body["operation"].ToString() != "-fullname" || command.GetConfiguration(i).ParametresCount() >= 3))
                                        {
                                            JArray values = new JArray();
                                            // Вбивает все переданные параметры
                                            for (j = 0; j < command.GetConfiguration(i).ParametresCount(); j++)
                                            {
                                                values.Add(command.GetConfiguration(i).GetParametr(j));
                                            }
                                            body["values"] = values;
                                            await controllerClient.PostAsync("user/info/write", body);
                                            success = true;
                                        }
                                    }
                                }
                            }
                            break;
                        case "/disciplines":
                            await controllerClient.PostAsync("disciplines", body);
                            success = true;
                            break;
                        case "/discipline":
                            // Работает только с конфигурацией
                            if (command.ConfigurationsCount() >= 1)
                            {
                                switch (command.GetConfiguration(0).Operation)
                                {
                                    case "-create":
                                        if (command.GetConfiguration(0).ParametresCount() >= 3)
                                        {
                                            body["title"] = command.GetConfiguration(0).GetParametr(0);
                                            body["describtion"] = command.GetConfiguration(0).GetParametr(1);
                                            body["teacher_id"] = command.GetConfiguration(0).GetParametr(2);
                                            await controllerClient.PostAsync("discipline/create", body);
                                            success = true;
                                        }
                                        break;
                                    case "-status":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            if (command.GetConfiguration(0).ParametresCount() >= 2)
                                            {
                                                body["status"] = command.GetConfiguration(0).GetParametr(1);
                                            }
                                            await controllerClient.PostAsync("discipline/status", body);
                                            success = true;
                                        }
                                        break;
                                    case "-info":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            if (command.GetConfiguration(0).ParametresCount() >= 3)
                                            {
                                                body["title"] = command.GetConfiguration(0).GetParametr(1);
                                                body["describtion"] = command.GetConfiguration(0).GetParametr(2);
                                            }
                                            await controllerClient.PostAsync("discipline/info", body);
                                            success = true;
                                        }
                                        break;
                                    case "-teachers":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            await controllerClient.PostAsync("discipline/teachers", body);
                                            success = true;
                                        }
                                        break;
                                    case "-tests":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            await controllerClient.PostAsync("discipline/tests", body);
                                            success = true;
                                        }
                                        break;
                                    case "-students":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            await controllerClient.PostAsync("discipline/students", body);
                                            success = true;
                                        }
                                        break;
                                    case "-addtest":
                                        if (command.GetConfiguration(0).ParametresCount() >= 2)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            body["title"] = command.GetConfiguration(0).GetParametr(1);
                                            await controllerClient.PostAsync("discipline/test/add", body);
                                            success = true;
                                        }
                                        break;
                                    case "-addteacher":
                                        if (command.GetConfiguration(0).ParametresCount() >= 2)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            body["user_id"] = command.GetConfiguration(0).GetParametr(1);
                                            body["user_type"] = "teacher";
                                            await controllerClient.PostAsync("discipline/user/add", body);
                                            success = true;
                                        }
                                        break;
                                    case "-sub":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            body["user_type"] = "student";
                                            await controllerClient.PostAsync("discipline/user/add", body);
                                            success = true;
                                        }
                                        break;
                                    case "-remuser":
                                        if (command.GetConfiguration(0).ParametresCount() >= 2)
                                        {
                                            body["discipline_id"] = command.GetConfiguration(0).GetParametr(0);
                                            body["user_id"] = command.GetConfiguration(0).GetParametr(1);
                                            await controllerClient.PostAsync("discipline/user/remove", body);
                                            success = true;
                                        }
                                        break;
                                }
                            }
                            break;
                        case "/questions":
                            await controllerClient.PostAsync("questions", body);
                            success = true;
                            break;
                        case "/question":
                            // Работает только с конфигурацией
                            if (command.ConfigurationsCount() >= 1)
                            {
                                switch (command.GetConfiguration(0).Operation)
                                {
                                    case "-create":
                                        if (command.GetConfiguration(0).ParametresCount() >= 2)
                                        {
                                            body["title"] = command.GetConfiguration(0).GetParametr(0);
                                            body["max_options"] = command.GetConfiguration(0).GetParametr(1);
                                            body["options"] = new JArray();
                                            body["points"] = new JArray();
                                            // Заполняем список опций ответов, начиная со второй конфигурации
                                            for (i = 1; i < command.ConfigurationsCount(); i++)
                                            {
                                                if (command.GetConfiguration(i).Operation != "-o" || command.GetConfiguration(i).ParametresCount() < 2)
                                                    continue;
                                                ((JArray)body["options"]).Add(command.GetConfiguration(i).GetParametr(0));
                                                ((JArray)body["points"]).Add(command.GetConfiguration(i).GetParametr(1));
                                            }
                                            await controllerClient.PostAsync("question/create", body);
                                            success = true;
                                        }
                                        break;
                                    case "-update":
                                        if (command.GetConfiguration(0).ParametresCount() >= 3)
                                        {
                                            body["question_id"] = command.GetConfiguration(0).GetParametr(0);
                                            body["title"] = command.GetConfiguration(0).GetParametr(1);
                                            body["max_options"] = command.GetConfiguration(0).GetParametr(2);
                                            body["options"] = new JArray();
                                            body["points"] = new JArray();
                                            // Заполняем список опций ответов, начиная со второй конфигурации
                                            for (i = 1; i < command.ConfigurationsCount(); i++)
                                            {
                                                if (command.GetConfiguration(i).Operation != "-o" || command.GetConfiguration(i).ParametresCount() < 2)
                                                    continue;
                                                ((JArray)body["options"]).Add(command.GetConfiguration(i).GetParametr(0));
                                                ((JArray)body["points"]).Add(command.GetConfiguration(i).GetParametr(1));
                                            }
                                            await controllerClient.PostAsync("question/update", body);
                                            success = true;
                                        }
                                        break;
                                    case "-status":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["question_id"] = command.GetConfiguration(0).GetParametr(0);
                                            if (command.GetConfiguration(0).ParametresCount() >= 2)
                                                body["question_version"] = command.GetConfiguration(0).GetParametr(1);
                                            if (command.GetConfiguration(0).ParametresCount() > 2)
                                                body["status"] = command.GetConfiguration(0).GetParametr(2);
                                            await controllerClient.PostAsync("question/status", body);
                                            success = true;
                                        }
                                        break;
                                    case "-info":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["question_id"] = command.GetConfiguration(0).GetParametr(0);
                                            if (command.GetConfiguration(0).ParametresCount() >= 2)
                                                body["question_version"] = command.GetConfiguration(0).GetParametr(1);
                                            await controllerClient.PostAsync("question/info", body);
                                            success = true;
                                        }
                                        break;
                                }
                            }
                            break;
                        case "/test":
                            // Работает только с конфигурацией
                            if (command.ConfigurationsCount() >= 1)
                            {
                                switch (command.GetConfiguration(0).Operation)
                                {
                                    case "-status":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["test_id"] = command.GetConfiguration(0).GetParametr(0);
                                            if (command.GetConfiguration(0).ParametresCount() >= 2)
                                            {
                                                body["status"] = command.GetConfiguration(0).GetParametr(1);
                                            }
                                            await controllerClient.PostAsync("test/status", body);
                                            success = true;
                                        }
                                        break;
                                    case "-info":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["test_id"] = command.GetConfiguration(0).GetParametr(0);
                                            if (command.GetConfiguration(0).ParametresCount() >= 3)
                                            {
                                                body["title"] = command.GetConfiguration(0).GetParametr(1);
                                                body["describtion"] = command.GetConfiguration(0).GetParametr(2);
                                            }
                                            await controllerClient.PostAsync("test/info", body);
                                            success = true;
                                        }
                                        break;
                                    case "-tries":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["test_id"] = command.GetConfiguration(0).GetParametr(0);
                                            if (command.GetConfiguration(0).ParametresCount() >= 2)
                                            {
                                                body["user_id"] = command.GetConfiguration(0).GetParametr(1);
                                            }
                                            await controllerClient.PostAsync("test/tries", body);
                                            success = true;
                                        }
                                        break;
                                    case "-modify":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["test_id"] = command.GetConfiguration(0).GetParametr(0);
                                            await controllerClient.PostAsync("test/modify", body);
                                            success = true;
                                        }
                                        break;
                                    case "-addques":
                                        if (command.GetConfiguration(0).ParametresCount() >= 2)
                                        {
                                            body["test_id"] = command.GetConfiguration(0).GetParametr(0);
                                            body["question_id"] = command.GetConfiguration(0).GetParametr(1);
                                            if (command.GetConfiguration(0).ParametresCount() >= 3)
                                            {
                                                body["question_version"] = command.GetConfiguration(0).GetParametr(2);
                                            }
                                            await controllerClient.PostAsync("test/question/add", body);
                                            success = true;
                                        }
                                        break;
                                    case "-remques":
                                        if (command.GetConfiguration(0).ParametresCount() >= 2)
                                        {
                                            body["test_id"] = command.GetConfiguration(0).GetParametr(0);
                                            body["question_number"] = command.GetConfiguration(0).GetParametr(1);
                                            await controllerClient.PostAsync("test/question/remove", body);
                                            success = true;
                                        }
                                        break;
                                    case "-quesseq":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["test_id"] = command.GetConfiguration(0).GetParametr(0);
                                            body["questions_signatures"] = new JArray();
                                            // Заполняем список вопросов, начиная со второй конфигурации
                                            for (i = 1; i < command.ConfigurationsCount(); i++)
                                            {
                                                if (command.GetConfiguration(i).Operation != "-q") continue;
                                                ((JArray)body["questions_signatures"]).Add(new JArray());
                                                if (command.GetConfiguration(i).ParametresCount() >= 1)
                                                    ((JArray)((JArray)body["questions_signatures"])[i-1]).Add(command.GetConfiguration(i).GetParametr(0));
                                                if (command.GetConfiguration(i).ParametresCount() >= 2)
                                                    ((JArray)((JArray)body["questions_signatures"])[i-1]).Add(command.GetConfiguration(i).GetParametr(1));
                                            }
                                            await controllerClient.PostAsync("test/questions/sequence", body);
                                            success = true;
                                        }
                                        break;
                                }
                            }
                            break;
                        case "/try":
                            // Работает только с конфигурацией
                            if (command.ConfigurationsCount() >= 1)
                            {
                                switch (command.GetConfiguration(0).Operation)
                                {
                                    case "-start":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["test_id"] = command.GetConfiguration(0).GetParametr(0);
                                            await controllerClient.PostAsync("try/start", body);
                                            success = true;
                                        }
                                        break;
                                    case "-stop":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["try_id"] = command.GetConfiguration(0).GetParametr(0);
                                            await controllerClient.PostAsync("try/stop", body);
                                            success = true;
                                        }
                                        break;
                                    case "-view":
                                        if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["try_id"] = command.GetConfiguration(0).GetParametr(0);
                                            await controllerClient.PostAsync("try/view", body);
                                            success = true;
                                        }
                                        break;
                                }
                            }
                            break;
                        case "/answer":
                            // Работает только с конфигурацией
                            if (command.ConfigurationsCount() >= 1)
                            {
                                // Для каждой операции -save, -delete
                                for (i = 0; i < command.ConfigurationsCount(); i++)
                                {
                                    // Скип, если нет параметров
                                    if (command.GetConfiguration(i).ParametresCount() < 1 ||
                                        (command.GetConfiguration(i).Operation != "-save" && command.GetConfiguration(i).Operation != "delete"))
                                        continue;

                                    body["answer_id"] = command.GetConfiguration(i).GetParametr(0);
                                    body["options"] = new JArray();
                                    switch (command.GetConfiguration(i).Operation)
                                    {
                                        case "-save":
                                            // Заполняет ответы со второго параметра
                                            for (j = 1; j < command.GetConfiguration(i).ParametresCount(); j++)
                                            {
                                                ((JArray)body["options"]).Add(command.GetConfiguration(i).GetParametr(j));
                                            }
                                            break;
                                        case "-delete": // ничего, сбстн
                                            break;
                                    }

                                    await controllerClient.PostAsync("answer/change", body);
                                    success = true;
                                }
                            }
                            break;
                        case "/news":
                            if (command.ConfigurationsCount() == 0)
                            {
                                await controllerClient.PostAsync("news", body);
                                    success = true;
                            }
                            else
                            {
                                switch (command.GetConfiguration(0).Operation)
                                {
                                    case "-status":
                                    if (command.GetConfiguration(0).ParametresCount() >= 1)
                                        {
                                            body["news_id"] = command.GetConfiguration(0).GetParametr(0);
                                            if (command.GetConfiguration(0).ParametresCount() >= 2)
                                            {
                                                body["status"] = command.GetConfiguration(0).GetParametr(1);
                                            }
                                            await controllerClient.PostAsync("news/status", body);
                                            success = true;
                                        }
                                    break;
                                }
                            }
                        break;
                    }
                
                    if (!success) await controllerClient.PostAsync("unknown", body);
                    
                    Console.WriteLine("Обработка команды окончена.");
                } 
            }

            public Task HandleErrorAsync(ITelegramBotClient bot, Exception exception, HandleErrorSource source, CancellationToken token)
            {
                Console.WriteLine($"[Polling Error] {exception.Message}");
                return Task.CompletedTask;
            }
        }

        // Структура для сборки команд
        public struct Command {

            public struct Configuration {
                public Configuration(string operation, List<string> parametres) {
                    Initialized = true;
                    Operation = operation;
                    Parametres = parametres;
                }

                public bool Initialized {get; private set;}
                public string Operation {get; private set;}
                private List<string> Parametres{get; set;}

                public int ParametresCount()
                {
                    return Parametres.Count;
                }
                public string GetParametr(int index)
                {
                    return Parametres[index];
                }

                public override string ToString()
                {
                    return $"{{ Operation: {Operation}, Parametres: {string.Join(", ", Parametres)} }}";
                }
            }

            public Command(string action, List<Configuration> configurations) {
                Initialized = true;
                Action = action;
                Configurations = configurations;
            }

            public bool Initialized {get;private set;}
            public string Action {get; private set;}
            private List<Configuration> Configurations {get; set;} 

            public int ConfigurationsCount()
            {
                return Configurations.Count;
            }
            public Configuration GetConfiguration(int index) {
                return Configurations[index];
            }
            public Configuration GetConfiguration(string operation) {
                return Configurations.FirstOrDefault(configuration => configuration.Operation == operation);
            }

            public override string ToString()
            {
                return $"{{ Action: {Action}, Configurations: {string.Join(", ", Configurations)} }}";
            }

            // Создание структуры команды из строки
            public static List<Command> Parse(string command)
            {
                // Если начинается не с /, то неее
                if (command[0] != '/') return null;
                // Готовим список для частей
                List<string> parts = new List<string>();
                // Получаем части команды
                bool inQuotes = false;
                string part = "";
                foreach (char symbol in command)
                {
                    if (symbol == '"')
                    {
                        // Сохраняются все " (даже внешние)
                        part += symbol;
                        inQuotes = !inQuotes;
                    }
                    else if (symbol == ' ')
                    {
                        if (inQuotes) part += symbol;
                        else if (part != "")
                        {
                            parts.Add(part);
                            part = "";
                        }
                    }
                    else part += symbol;
                }
                if (part != "") { parts.Add(part); part = ""; } // Добавление последней части

                // Делим части по командам
                List<Command> commands = new List<Command>();
                for (int i = 0; i < parts.Count;)
                {
                    // Проверяем, что часть - команда
                    if (parts[i][0] == '/')
                    {
                        // Сохраняем действие и переходим к следующей части
                        string action = parts[i];
                        i++;

                        // Собираем конфигурации
                        List<Command.Configuration> configurations = new List<Command.Configuration>();
                        // Если часть - конфигурация
                        while (i < parts.Count && parts[i][0] == '-')
                        {
                            // Сохраняем операцию и переходим к следующей части
                            string operation = parts[i];
                            i++;

                            // Собираем все параметры далее
                            List<string> parametres = new List<string>();
                            while (i < parts.Count && parts[i][0] != '-' && parts[i][0] != '/')
                            {
                                // Сохраняем параметр и переходим к следующей части
                                string parametr = parts[i];
                                if (parametr == "" || parametr == "\"") parametr = "_"; // Защита от пустого параметра и выхода за пределы при обрезке
                                else if ((parametr[0] == '\"' && parametr[parametr.Length - 1] == '\"') ||
                                    parametr[0] == '"' && parametr[parametr.Length - 1] == '"') // Если параметр в кавычках
                                    parametr = parametr.Substring(1, parametr.Length - 2).Replace("\"\"", "\""); // Обрезаем кавычки и убираем дублирование
                                parametres.Add(parametr);
                                i++;
                            }
                            configurations.Add(new Command.Configuration(operation, parametres));
                        }
                        commands.Add(new Command(action, configurations));
                    }
                    else i++;
                }

                return commands;
            }
        }

        private readonly List<BotController> botControllers = new List<BotController>();
        private readonly ITelegramBotClient botClient;
        private RedisStorage redisStorage;

        public ClientManager()
        {
            Console.WriteLine("Клиент создан.");
            // Инициализация Telegram-бота (Клиент для обращения)
            botClient = new TelegramBotClient(ConfigLoader.Get("BOT_TOKEN"));
            // Подключение к редис
            redisStorage = new RedisStorage(ConfigLoader.Get("REDIS_URI"));
        }

        private Task CreateAndStartNewBotController(string host, int port) {
            // Создание бота с тем же клиентом
            BotController controller = new BotController(botClient, redisStorage);
            botControllers.Add(controller);
            // Запускает в параллели
            return Task.Run(async () => controller.StartAsync(host, port));
        }

        // Запуск бота
        public async Task ListenAsync()
        {
            // Создание контроллера
            int controllers_count = int.Parse(ConfigLoader.Get("SERVERS_COUNT"));
            string controller_host = ConfigLoader.Get("SERVER_HOST");
            int controller_port = int.Parse(ConfigLoader.Get("SERVER_PORT"));
            Console.WriteLine($"Запускаем контроллеры ({controllers_count})");
            for (int i = 0; i < controllers_count; i++)
            {
                CreateAndStartNewBotController(controller_host, controller_port + i + 1);
            }
            // Создаёт источник сигнала отмены
            var cts = new CancellationTokenSource();
            // Запускает механизм получения обновлений от Telegram
            Console.WriteLine("Запускаем работу клиента");
            IUpdateHandler updateHandler = new ClientUpdateHandler();
            botClient.StartReceiving(
                // Указывает, какой метод будет обрабатывать входящие сообщения
                updateHandler: updateHandler,
                receiverOptions: new ReceiverOptions
                    {
                        Limit = 10
                    },
                cancellationToken: cts.Token
            );
            Console.WriteLine("Клиент бота запущен (Long Polling способ)...");

            // Бесконечное ожидание
            try
            {
                await Task.Delay(Timeout.Infinite, cts.Token);
            }
            catch (OperationCanceledException)
            {
                Console.WriteLine("Работа клиента бота прервана по запросу.");
            }
        }
    }

}