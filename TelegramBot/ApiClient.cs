using System;
using System.Text;
using System.Linq;
using System.Net.Http;
using System.Collections.Generic;
using Newtonsoft.Json;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace TelegramBot {
    public class ApiClient
    {
        private readonly HttpClient httpClient;
        private readonly string serverURL;

        public ApiClient(string serverURL)
        {
            this.serverURL = serverURL;
            httpClient = new HttpClient();
        }

        public async Task<JObject> GetAsync(string endpoint, Dictionary<string, string> queryParams = null,
            string tokenPrefix = "", string token = "")
        {
            // Формируем базовый URL
            string url = $"{serverURL}/{endpoint}";
            // Если есть параметры — добавляем их в URL
            if (queryParams != null && queryParams.Count > 0)
            {
                var queryString = string.Join("&", queryParams.Select(p => 
                    $"{Uri.EscapeDataString(p.Key)}={Uri.EscapeDataString(p.Value)}"));
                url += $"?{queryString}";
            }
            Console.WriteLine("Отправка GET запроса на " + url);

            // Отправляем GET‑запрос
            HttpResponseMessage response = await httpClient.GetAsync(url);

            // Читаем и десериализуем ответ
            string responseContent = await response.Content.ReadAsStringAsync();
            JObject result = JObject.Parse(responseContent);
            result["status"] = response.IsSuccessStatusCode;
            return result;
        }

        public async Task<JObject> PostAsync(string endpoint, JObject body, Dictionary<string, string> queryParams = null,
            string tokenPrefix = "", string token = "")
        {
            // Формируем URL с query‑параметрами (если есть)
            string url = $"{serverURL}/{endpoint}";
            if (queryParams != null && queryParams.Count > 0)
            {
                string queryString = "";
                foreach (var pair in queryParams) {
                    queryString = (queryString == "" ? "" : "&") + Uri.EscapeDataString(pair.Key) + "=" + Uri.EscapeDataString(pair.Value);
                }

                url += $"?{queryString}";
            }
            Console.WriteLine("Отправка POST запроса на " + url);

            // Формируем content для запроса
            StringContent content = new StringContent(body.ToString(), Encoding.UTF8, "application/json");
            // Отправляем POST‑запрос
            HttpResponseMessage response = await httpClient.PostAsync(url, content);


            // Читаем и десериализуем ответ
            string responseContent = await response.Content.ReadAsStringAsync();
            JObject result = JObject.Parse(responseContent);
            result["status"] = response.IsSuccessStatusCode;
            return result;
        }
    }
}
