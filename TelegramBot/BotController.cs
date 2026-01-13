using System;
using System.IO;
using System.Text;
using Telegram.Bot;
using Telegram.Bot.Types;
using Telegram.Bot.Types.Enums;
using Telegram.Bot.Types.ReplyMarkups;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Builder;
using Microsoft.Extensions.Hosting;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace TelegramBot {
    public class BotController
    {
        private RedisStorage redisStorage;
        private ApiClient authClient;
        private ApiClient testClient;
        private ITelegramBotClient botClient;

        public BotController(ITelegramBotClient botClient)
        {
            // Инициализируем клиент бота
            this.botClient = botClient;
            // Подключается к редис
            redisStorage = new RedisStorage(ConfigLoader.Get("REDIS_URI"));
            // Создаёт клиенты Основного модуля и Авторизации
            authClient = new ApiClient(ConfigLoader.Get("AUTHORIZATION_MODULE_URL"));
            testClient = new ApiClient(ConfigLoader.Get("TEST_MODULE_URL"));
            Console.WriteLine("Контроллер команд создан.");
        }
        // await botClient.SendTextMessageAsync(chatId, message);

        // Добывает данные из тела
        public static async Task<JObject> GetDataFromBody(Stream body)
        {
            // Читаем в строку
            StreamReader reader = new StreamReader(body, Encoding.UTF8);
            string jsonString = await reader.ReadToEndAsync();
            Console.WriteLine("Тело прочтено: " + jsonString);
            reader.Dispose();
            // Преобразуем в JObject
            return JObject.Parse(jsonString);
        }
        // Делает ответ
        public static Task WriteResponseAsync(HttpContext context, int statusCode, string message)
        {
            context.Response.StatusCode = statusCode;
            context.Response.ContentType = "text/plain";
            return context.Response.WriteAsync(message);
        }

        public async Task StartAsync(string host, int port)
        {
            // Создаём билдер
            var builder = WebApplication.CreateBuilder();
            // Создаём сервер
            var app = builder.Build();

            // Связываем обработчики с маршрутами
            // Неизвестная команда
            app.MapPost("/unknown", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                long chatId = long.Parse(body["chat_id"].ToString());

                await botClient.SendMessage(chatId,
                @"Неизвестная команда. Введите /help для получения списка команд");
                
                await WriteResponseAsync(context, 200, "Информация отправлена.");
            });
            app.MapPost("/start", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                long chatId = long.Parse(body["chat_id"].ToString());

                await botClient.SendMessage(chatId,
                string.Join(Environment.NewLine, new[] {
                "Вас приветствует проект Latter.",
                "Введите /help для получения списка команд."
                }));
                
                await WriteResponseAsync(context, 200, "Информация отправлена.");
            });
            // Список команд
            app.MapPost("/help", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                long chatId = long.Parse(body["chat_id"].ToString());

                await botClient.SendMessage(chatId, "Вот список команд:");

                await botClient.SendMessage(chatId, 
                string.Join(Environment.NewLine, new[] {
                "/login (тип) (параметры) - Авторизоваться выбранным способом. Тип авторизации и запрашиваемые параметры:",
                "-default: почта, пароль;",
                "-code: почта, код (если не будет введён код, он сгенерируется и его можно будет увидеть в разделе news на авторизированном устройстве)",
                "-yandex: без параметров (будет отправлена ссылка на Яндекс OAuth авторизацию)",
                "-github: без параметров (будет отправлена ссылка на Github OAuth авторизацию)",
                }));

                await botClient.SendMessage(chatId, 
                string.Join(Environment.NewLine, new[] {
                "Примечания:",
                "1) Параметры передаются через пробел.",
                "2) Чтобы передать в качестве параметра текст, содержащий пробелы, его необходимо заключить в двойные кавычки. Если они присутствуют в тексте - их следует дублировать.",
                "3) Заключение параметра в двойные кавычки также необходимо, если он начинается с символа '-' или '/'"
                }));

                await WriteResponseAsync(context, 200, "Информация отправлена.");
            });

            // Формируем url
            string url = $"http://{host}:{port}";
            // Запускаем сервер для обработки команд
            Console.WriteLine($"Запускаем сервер контроллера на {url}.");
            app.Run(url);
            // Сообщаем о прекрашении работы сервера
            Console.WriteLine("Сервер контроллера остановлен.");
        }

        public static string GenerateRandomToken(int length = 10)
        {
            const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
            var token = new char[length];

            Random random = new Random();
            for (int i = 0; i < length; i++)
                token[i] = chars[random.Next(chars.Length)];

            return new string(token);
        }
    }
}