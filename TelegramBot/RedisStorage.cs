using StackExchange.Redis;
using System.Threading.Tasks;

namespace TelegramBot {
    public class RedisStorage
    {
        private readonly IConnectionMultiplexer redis;
        private readonly IDatabase database;

        public RedisStorage(string connectionString)
        {
            //redis = ConnectionMultiplexer.Connect(connectionString);
            //database = redis.GetDatabase();
        }

        // Сохранить значение
        public async Task SetAsync(string key, string value)
        {
            await database.StringSetAsync(key, value);
        }

        // Получить значение
        public async Task<string> GetAsync(string key)
        {
            var value = await database.StringGetAsync(key);
            return value.HasValue ? value.ToString() : null;
        }

        // Удалить значение
        public async Task RemoveAsync(string key)
        {
            await database.KeyDeleteAsync(key);
        }

        // Проверить существование ключа
        public async Task<bool> ExistsAsync(string key)
        {
            return await database.KeyExistsAsync(key);
        }
    }

}