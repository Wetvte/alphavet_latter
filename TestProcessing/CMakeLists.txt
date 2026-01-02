cmake_minimum_required(VERSION 3.10)
project(Latter)

# Указываем путь к глобальному vcpkg и подключаем его инструмент
set(CMAKE_TOOLCHAIN_FILE
# Путь к глобальному vcpkg, в который установлен jwt-cpp
    "C:/Program Files/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Путь к vcpkg.cmake")

# Стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Исходные файлы
file(GLOB SOURCES
    "main.cpp"
    "src/*.cpp"
    "config/*.env"
)

# Поиск зависимостей
# PostgreSQL
find_package(PostgreSQL REQUIRED)
# JWT-cpp (будет найден через vcpkg)
find_package(jwt-cpp REQUIRED)

# Исполняемый файл
add_executable(latter ${SOURCES})

# Директории с заголовками
target_include_directories(latter PRIVATE
    include
    ${PostgreSQL_INCLUDE_DIRS}
)
# Подключение библиотек
target_link_libraries(latter PRIVATE
    ${PostgreSQL_LIBRARIES}
    jwt-cpp::jwt-cpp
)
