using System;
using System.IO;
using DotNetEnv;
using System.Collections.Generic;

namespace TelegramBot {
    public static class ConfigLoader
    {
        // Загружает .env-файл
        public static void Load(string filePath = null)
        {
            Console.WriteLine("Находим конфиг...");
            // Если путь не указан — ищем .env в текущей директории
            var envPath = filePath ?? ".env";

            if (!System.IO.File.Exists(envPath))
                throw new FileNotFoundException($"Файл .env не найден по пути: {envPath}");

            Console.WriteLine("Загружаем конфиг...");
            Env.Load(envPath); // Загружаем переменные в окружение процесса
            Console.WriteLine("Конфиг загружен.");
        }

        // Возвращает значение переменной из .env
        public static string Get(string key)
        {
            return Environment.GetEnvironmentVariable(key);
        }

        /// Возвращает значение или бросает исключение, если не найдено
        public static string GetRequired(string key)
        {
            var value = Get(key);
            if (value == null)
                throw new KeyNotFoundException($"Переменная '{key}' не найдена в .env");
            return value;
        }

        /// Возвращает значение или дефолтное, если не найдено
        public static string GetOrDefault(string key, string defaultValue)
        {
            return Get(key) ?? defaultValue;
        }

        /// Проверяет, существует ли переменная
        public static bool Has(string key)
        {
            return !string.IsNullOrEmpty(Get(key));
        }
    }

}