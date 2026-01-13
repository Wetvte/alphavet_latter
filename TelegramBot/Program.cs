using System;
using Telegram.Bot;
using System.Threading.Tasks;

namespace TelegramBot {
    class Program
    {
        private static async Task Main(string[] args)
        {
            try
            {
                // Загрузка конфигурации из .env
                ConfigLoader.Load("C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\TelegramBot\\config\\bot_config.env");

                // Создаёт клиент
                ClientManager clientManager = new ClientManager();
                // Запуск клиента бота и получения сообщений
                await clientManager.ListenAsync();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка запуска сервера: {ex.Message}");
            }
        }
    }
}