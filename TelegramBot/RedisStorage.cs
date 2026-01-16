using System;
using StackExchange.Redis;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace TelegramBot {
    public class RedisStorage
    {
        private readonly IConnectionMultiplexer redis;
        private readonly IDatabase database;

        public RedisStorage(string connectionString)
        {
            redis = ConnectionMultiplexer.Connect(connectionString);
            database = redis.GetDatabase();
            Console.WriteLine("Обработчик подключен к Redis.");
        }

        // Сохранить значение
        public async Task SetAsync(long key, JObject value)
        {
            await database.StringSetAsync(key.ToString(), value.ToString());
            Console.WriteLine($"По ключу {key} сохранено значение {value}");
        }

        // Получить значение
        public async Task<JObject> GetAsync(long key)
        {
            var value = await database.StringGetAsync(key.ToString());
            string result = value.HasValue ? value.ToString() : "{}";
            Console.WriteLine($"По ключу {key} получено значение {result}");
            return JObject.Parse(result);
        }
        public async Task<JObject> GetAuthorizeCheckedAsync(long key)
        {
            var value = await database.StringGetAsync(key.ToString());
            string result = value.HasValue ? value.ToString() : "{}";
            Console.WriteLine($"По ключу {key} получено значение {result}");
            JObject user = JObject.Parse(result);
            user["authorized"] = (value.HasValue && user.ContainsKey("status") && user["status"].ToString() == "Authorized") ? "true" : "false";
            return user;
        }

        // Удалить значение
        public async Task RemoveAsync(long key)
        {
            await database.KeyDeleteAsync(key.ToString());
        }

        // Проверить существование ключа
        public async Task<bool> ExistsAsync(long key)
        {
            return await database.KeyExistsAsync(key.ToString());
        }
    }

}