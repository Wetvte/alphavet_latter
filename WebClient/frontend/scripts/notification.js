let notification;
let notificationMessage;

// Функция показа уведомления
// type: "error", "success"
function send_notification(type, message, timeoutSeconds = 5) {
  if (!notification) {
    notification = document.getElementById('notification');
    notificationMessage = document.getElementById('notificationMessage');
  }

  notificationMessage.textContent = message;
  notification.className = `notification ${type} show`;

  // Автоматическое скрытие
  setTimeout(() => {
    notification.classList.remove('show');
  }, timeoutSeconds * 1000);
}