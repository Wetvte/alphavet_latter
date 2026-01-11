// Настройка секций
function set_section_toggle(container, button, value_name, state) {
    container.is_open = state;
    container.style.display = state ? "block" : "none";
    set_button_toggle(button, value_name, state);
  }
  function set_button_toggle(button, value_name, state) {
    button.is_open = state;
    button.textContent = (state ? "Скрыть" : "Показать") + " " + value_name;
  }