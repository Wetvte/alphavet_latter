const redis = require('redis');


const client = redis.createClient({
  url: 'redis://default:P9kDtXtI4vwwsPzlyuv0JFJxIyE4bx9d@redis-16791.c253.us-central1-1.gce.cloud.redislabs.com:16791'
});


client.on('error', (err) => console.log('Error:', err));
client.on('connect', () => console.log('Connected!'));

(async () => {
  await client.connect();
  console.log(await client.ping()); // Должно вывести 'PONG'
  await client.set('test:key', 'Привет, Redis!');
  const value = await client.get('test:key');
  console.log('Получено:', value);
  await client.quit();
})();
