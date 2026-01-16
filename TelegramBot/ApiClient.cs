using System;
using System.Text;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Collections.Generic;
using Newtonsoft.Json;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace TelegramBot {
    public class ApiClient
    {
        public struct Response
        {
            public Response(int status, JObject body) {
                Status = status;
                Body = body;
            }

            public int Status {get; private set;}
            public JObject Body {get; private set;}
        }
        public class Token
        {
            public class RefreshData
            {
                public RefreshData(string refreshToken, RedisStorage storage, long key, bool removeFromStorageOnFail, ApiClient client = null, string endpoint = "refresh")
                {
                    Enabled = true;
                    RefreshToken = refreshToken;
                    Storage = storage;
                    Key = key;
                    RemoveFromStorageOnFail = removeFromStorageOnFail;
                    AuthClient = client;
                    Endpoint = endpoint;
                }

                public bool Enabled { get; private set; }
                public string RefreshToken { get; private set; }
                public bool RemoveFromStorageOnFail { get; private set; }
                public RedisStorage Storage { get; private set; }
                public long Key { get; private set; }
                public ApiClient AuthClient { get; private set; }
                public string Endpoint { get; private set; }

                public async Task<string> Refresh(string prefix, string token)
                {
                    // Обрабатываем отсутствие записи
                    bool exists = await Storage.ExistsAsync(Key);
                    if (!exists) return "";

                    // Если клиент не был задан, создаём свой
                    if (AuthClient == null) AuthClient = new ApiClient(ConfigLoader.Get("AUTHORIZATION_MODULE_URL"));
                    Response response = await AuthClient.GetAsync(Endpoint, null, new Token(prefix, RefreshToken));

                    // Успешное обновление токенов
                    if (response.Status == 200)
                    {
                        RefreshToken = response.Body["refresh_token"].ToString();
                        string new_token = response.Body["access_token"].ToString();

                        JObject user = await Storage.GetAsync(Key);
                        user["refresh_token"] = RefreshToken;
                        user["access_token"] = new_token;
                        await Storage.SetAsync(Key, user);
                        return new_token;
                    }
                    else if (RemoveFromStorageOnFail)
                    {
                        await Storage.RemoveAsync(Key);
                    }

                    return "";
                }
            }

            public Token(string prefix, string value)
            {
                Initialized = true;
                Prefix = prefix;
                Value = value;
                Refreshable = false;
            }
            public Token(string prefix, string value, RefreshData data)
            {
                Initialized = true;
                Prefix = prefix;
                Value = value;
                Refreshable = true;
                refreshData = data;
            }

            public bool Initialized { get; private set; }
            public string Prefix { get; private set; }
            public string Value { get; private set; }
            public bool Refreshable { get; private set; }
            private RefreshData refreshData;

            public async Task Refresh()
            {
                if (!Refreshable || !refreshData.Enabled) Value = "";
                else Value = await refreshData.Refresh(Prefix, Value);

                if (Value == "")
                {
                    Initialized = false;
                    Refreshable = false;
                }
            }
        }

        private readonly HttpClient httpClient;
        private readonly string serverURL;

        public ApiClient(string serverURL)
        {
            this.serverURL = serverURL;
            httpClient = new HttpClient(new HttpClientHandler { UseProxy = false });
        }

        public async Task<Response> GetAsync(string endpoint, Dictionary<string, string> queryParams = null, Token token = null)
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

            // Создаём запрос с заголовками
            HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, url);
            // Устанавливаем токен в заголовок Authorization, если он передан
            if (token != null && token.Initialized)
            {
                request.Headers.Authorization = new AuthenticationHeaderValue(token.Prefix, token.Value);
                    Console.WriteLine("Старый " + request.Headers.Authorization);
            }

            // Отправляем GET‑запрос
            HttpResponseMessage response = await httpClient.SendAsync(request);
            // Если нужно, в случае отказа токена, обновить его
            if ((int)response.StatusCode == 401 && token.Refreshable)
            {
                await token.Refresh();
                if (token != null && token.Initialized)
                {
                    // Меняем запрос и заголовок
                    request = new HttpRequestMessage(HttpMethod.Get, url);
                    request.Headers.Authorization = new AuthenticationHeaderValue(token.Prefix, token.Value);
                    Console.WriteLine("Новый " + request.Headers.Authorization);
                    // Отправляем ещё раз
                    response = await httpClient.SendAsync(request);
                }
            }
            
            // Читаем и десериализуем ответ
            string responseContent = await response.Content.ReadAsStringAsync();
            JObject result = responseContent == "" ? new JObject() : JObject.Parse(responseContent);
            return new Response((int)response.StatusCode, result);
        }

        public async Task<Response> PostAsync(string endpoint, JObject body, Dictionary<string, string> queryParams = null, Token token = default)
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
            StringContent content = new StringContent(body == null ? "" : body.ToString(), Encoding.UTF8, "application/json");

            // Создаём запрос с заголовками
            HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, url) { Content = content };

            // Устанавливаем токен в заголовок Authorization, если он передан
            if (token != null && token.Initialized)
            {
                request.Headers.Authorization = new AuthenticationHeaderValue(token.Prefix, token.Value);
            }
            request.Headers.Add("Accept", "application/json");

            // Отправляем POST‑запрос
            HttpResponseMessage response = await httpClient.SendAsync(request);

            // Если нужно, в случае отказа токена, обновить его
            if ((int)response.StatusCode == 401 && token.Refreshable)
            {
                await token.Refresh();
                if (token != null && token.Initialized)
                {
                    // Меняем запрос и заголовок
                    request = new HttpRequestMessage(HttpMethod.Post, url) { Content = content };
                    request.Headers.Authorization = new AuthenticationHeaderValue(token.Prefix, token.Value);
                    // Отправляем ещё раз
                    response = await httpClient.SendAsync(request);
                }
            }
            
            // Читаем и десериализуем ответ
            string responseContent = await response.Content.ReadAsStringAsync();
            JObject result = responseContent == "" ? new JObject() : JObject.Parse(responseContent);
            return new Response((int)response.StatusCode, result);
        }
    }
}
