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

            public ClientUpdateHandler() {
                // Создание клиента контроллера (ов)
                controllerClient = new ApiClient($"http://{ConfigLoader.Get("SERVER_HOST")}:{ConfigLoader.Get("SERVER_PORT")}");
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
                // Создаём тело запроса
                JObject body = new JObject();
                body["chat_id"] = chatId;
                // Парсим текст в команды
                List<Command> commands = ClientManager.Command.Parse(text);
                Console.WriteLine("Интерпретировано, как команды: " + string.Join(", ", commands));
                // Обрабатывает каждую команду
                for (int i = 0; i < commands.Count; i++) {
                    // Обработка команды
                    Console.WriteLine("Обработка команды: " + commands[i].Action);
                    switch(commands[i].Action) {
                        case "/start":
                            await controllerClient.PostAsync("start", body);
                        break;
                        case "/help":
                            await controllerClient.PostAsync("help", body);
                        break;
                        default:
                            await controllerClient.PostAsync("unknown", body);
                        break;
                    }
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

                public string GetParametr(int index)
                {
                    return Parametres[index];
                }

                public override string ToString()
                {
                    return $"{{Operation: {Operation}, Parametres: {string.Join(", ", Parametres)}}}";
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

            public Configuration GetConfiguration(int index) {
                return Configurations[index];
            }
            public Configuration GetConfiguration(string operation) {
                return Configurations.FirstOrDefault(configuration => configuration.Operation == operation);
            }

            public override string ToString()
            {
                return $"{{Action: {Action}, Configurations: {string.Join(", ", Configurations)}}}";
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
                    { // Вставляем первую кавычку, вторую не вставляем.
                        // Сохраняются все " (даже внешние), но повторение заменится одним символом
                        if (!inQuotes) part += symbol;
                        inQuotes = !inQuotes;
                    }
                    if (symbol == ' ')
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
                        if (i < parts.Count && parts[i][0] == '-')
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
                                if (parametr == "" || parametr == "\"") parametr = "_"; // Защита от пустого параметра
                                else if (parametr[0] == '"' && parametr[parts.Count - 1] == '"')
                                    parametr.Substring(1, parametr.Length - 2); // Обрезаем кавычки
                                parametres.Add(parametr);
                                i++;
                            }
                            configurations.Add(new Command.Configuration(operation, parametres));
                        }
                        commands.Add(new Command(action, configurations));
                    }
                }

                return commands;
            }
        }

        private readonly List<BotController> botControllers = new List<BotController>();
        private readonly ITelegramBotClient botClient;

        public ClientManager()
        {
            Console.WriteLine("Сервер создан.");
            // Инициализация Telegram-бота (Клиент для обращения)
            botClient = new TelegramBotClient(ConfigLoader.Get("BOT_TOKEN"));
        }

        private Task CreateAndStartNewBotController(string host, int port) {
            // Создание бота с тем же клиентом
            BotController controller = new BotController(botClient);
            botControllers.Add(controller);
            // Запускает в параллели
            return Task.Run(async () => await controller.StartAsync(host, port));
        }

        // Запуск бота
        public async Task ListenAsync()
        {
            // Создание контроллера
            CreateAndStartNewBotController(ConfigLoader.Get("SERVER_HOST"), int.Parse(ConfigLoader.Get("SERVER_PORT")));
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