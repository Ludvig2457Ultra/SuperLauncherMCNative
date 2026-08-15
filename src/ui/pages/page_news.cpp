#include "pages_priv.h"
#include <cstdio>

namespace slui {

HWND create_news_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    p->data = new int(0); // dummy
    int x = 40, y = 30, cw = 680;
    MakeLabel(h, 0, "Новости", x, y, cw, 30);
    y += 44;
    HWND memo = MakeMemo(h, 0, x, y, cw, 460);
    SetCtrl(memo,
        "2025-08-12 v1.4.0.7 — Добавлен Discord RPC\n"
        "2025-07-24 v1.4.0.5 — Поддержка скачивания модов из Modrinth и настройки лаунчера\n"
        "2025-07-23 v1.4.0.4 — Создание и управление Minecraft-серверами\n"
        "2025-07-23 v1.4.0.3 — Новый дизайн и восстановлен код\n"
        "2025-06-26 v1.4.0.2 — Новый дизайн, но утерян код\n"
        "2025-06-26 v1.3 — Лаунчер выйдет из беты");
    return h;
}

} // namespace slui