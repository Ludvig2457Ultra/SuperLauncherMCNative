#pragma once
#include <string>
#include <vector>

// Кроссплатформенные операции (Win + Unix). Всё, что в этом слое, не зависит
// от Win32: на Windows часть функций делегируется в WinAPI, на других ОС — в
// POSIX/STL. Используется, чтобы ядро (установка/запуск) собиралось без UI.
namespace sl {

// Значение переменной окружения (пустая строка, если нет).
std::string env_get(const char* name);

// Домашний каталог пользователя: Win — %USERPROFILE%, Unix — $HOME.
std::string env_user_home();

// Список записей каталога (без "." и ".."), отсортированный по алфавиту.
std::vector<std::string> list_directory(const std::string& path);

// Запустить команду через shell и дождаться завершения (Unix-путь: fork+exec
// /bin/sh -c, Windows-путь: CreateProcessA). Возвращает код выхода;
// 0 — успех, -1 — ошибка запуска (в err — причина).
int run_command_and_wait(const std::string& cmdline, const std::string& cwd,
                         std::string* err);

} // namespace sl