#pragma once
#include "pages.h"
#include "../ui.h"
#include "../../core/common.h"
#include <string>
#include <vector>
#include <functional>

// Внутренние утилиты страниц.

namespace slui {

// Заполнить комбо списком строк.
void sl_fill_combo(HWND combo, const std::vector<std::string>& items, int select = 0);

// Получить текст выбранного элемента комбо.
std::string sl_combo_sel(HWND combo);

// Добавить строку в memo (с автоскроллом вниз).
void sl_memo_append(HWND memo, const std::string& s);

// Очистить listbox и заполнить.
void sl_fill_list(HWND list, const std::vector<std::string>& items);

// Показать MessageBox-диалог (title, msg, icon MB_ICONINFO/MBWARNING/MB_ICONERROR).
void sl_msg(HWND parent, const std::string& title, const std::string& msg, UINT flags);

// Подтверждение: true если пользователь согласен.
bool sl_confirm(HWND parent, const std::string& title, const std::string& msg);

// Быстрый запуск рабочего потока (не владеет жизнью).
void sl_thread(std::function<void()> fn);

// Заполнить комбо моделей AI (владеет list).
void sl_ai_set_models(HWND combo, std::vector<std::string>* list);

// Диалоги (src/ui/dialogs/dialogs.cpp)
void sl_show_login_dialog(HWND parent);
void sl_show_create_server_dialog(HWND parent);

} // namespace slui