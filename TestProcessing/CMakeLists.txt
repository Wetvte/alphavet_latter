cmake_minimum_required(VERSION 3.10)

# Указываем путь к глобальному vcpkg и подключаем его инструмент
set(CMAKE_TOOLCHAIN_FILE
# Путь к глобальному vcpkg, в который установлен jwt-cpp
    "C:/Program Files/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Путь к vcpkg.cmake")

# Стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_GENERATOR_PLATFORM x64)

# Задаём кодировку для компилятора
if(MSVC)
    add_compile_options("/utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

project(Latter)

# Исходные файлa
file(GLOB SOURCES
    "main.cpp"
    "src/*.cpp"
)

# Поиск зависимостей
# PostgreSQL
find_package(PostgreSQL REQUIRED)
# JWT-cpp (будет найден через vcpkg)
find_package(jwt-cpp REQUIRED 
CONFIG
PATHS "C:/Program Files/vcpkg/installed/x64-windows/share/jwt-cpp") # Полный путь до пакета jwt-cpp

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

# В папке build
# cmake -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows ..
# cmake --build .