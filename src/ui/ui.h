#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace slui {

struct PageEvent;

// Тёмная тема и создание контролов (используются страницами).
COLORREF page_bg_color();
COLORREF page_bg_top();
COLORREF page_bg_bottom();

// Вертикальный градиент (banded) по области rc.
void ui_fill_gradient_v(HDC dc, RECT* rc, COLORREF top, COLORREF bottom, int bands = 48);
// Горизонтальный градиент (banded) по области rc.
void ui_fill_gradient_h(HDC dc, RECT* rc, COLORREF left, COLORREF right, int bands = 32);

// Заливка/обводка скруглённого прямоугольника (используются страницами).
void FillRound(HDC dc, const RECT& rc, int radius, COLORREF color);
void StrokeRound(HDC dc, const RECT& rc, int radius, COLORREF color);

HWND MakeLabel(HWND parent, int id, const std::string& text, int x, int y, int w, int h);
// Заголовок страницы (жирный, акцентный).
HWND MakeTitle(HWND parent, int id, const std::string& text, int x, int y, int w, int h);
// Подзаголовок секции (полужирный, приглушённый).
HWND MakeSub(HWND parent, int id, const std::string& text, int x, int y, int w, int h);
HWND MakeEdit(HWND parent, int id, const std::string& text, int x, int y, int w, int h);
HWND MakeCombo(HWND parent, int id, int x, int y, int w, int h);
HWND MakeProgress(HWND parent, int id, int x, int y, int w, int h);
HWND MakeMemo(HWND parent, int id, int x, int y, int w, int h);
HWND MakeList(HWND parent, int id, int x, int y, int w, int h);

void SetCtrl(HWND c, const std::string& s);
std::string GetCtrl(HWND c);

// Кнопка в стиле темы (тёмная, акцент-полоса при фокусе).
HWND MakeButton(HWND parent, int id, const std::string& text, int x, int y, int w, int h);

// Акцентная (primary) кнопка — залита фирменным цветом.
HWND MakeButtonAccent(HWND parent, int id, const std::string& text, int x, int y, int w, int h);

// Кисти темы для WM_CTLCOLOR* в обработчиках страниц.
HBRUSH ui_bg_brush();
HBRUSH ui_edit_brush();

struct Win {
    HWND h = nullptr;
    Win() = default;
    Win(HWND hwnd) : h(hwnd) {}
    void set_text(const std::string& s);
    std::string get_text() const;
    void show(bool on = true);
    void move(int x, int y, int w, int h);
};

// Сообщения от рабочего потока к главному окну
enum { WM_SL_STATUS = WM_APP + 1, WM_SL_PROGRESS = WM_APP + 2, WM_SL_DONE = WM_APP + 3,
       WM_SL_VERSIONS = WM_APP + 4, WM_SL_EVENT = WM_APP + 5 };

struct LauncherApp {
    HINSTANCE hinst = nullptr;
    HWND hwnd = nullptr;
    HWND sidebar = nullptr;
    HWND content = nullptr;
    std::vector<HWND> nav;          // кнопки сайдбара
    std::vector<HWND> pages;        // страницы
    int cur_page = 0;
    bool working = false;           // идёт установка/запуск

    bool init(HINSTANCE hinst, int cmdshow);
    void run();
    void show_page(int idx);
    void layout();
    void on_nav(HWND clicked);
};

int run_app(HINSTANCE hinst, int cmdshow);

} // namespace slui