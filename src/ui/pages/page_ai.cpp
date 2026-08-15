#include "pages_priv.h"
#include "../../backend/ai.h"
#include "../../core/config.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cstdio>

namespace slui {

enum {
    ID_AI_ENDPOINT = 3001, ID_AI_MODEL = 3002, ID_AI_SEND = 3003, ID_AI_CHAT = 3004,
    ID_AI_INPUT = 3005, ID_AI_SCAN = 3006, ID_AI_TOGGLE = 3007,
};

struct AIData {
    HWND endpoint = 0, model_combo = 0, chat = 0, input = 0, quantum_btn = 0;
    bool quantum = false;
    bool busy = false;
    std::string base_url;   // http://localhost:11434/v1
    std::string api_key;
};

static AIData* A(Page* p) { return (AIData*)p->data; }

static std::string current_model(AIData* d) {
    int sel = (int)ComboBox_GetCurSel(d->model_combo);
    if (sel >= 0) {
        int len = (int)ComboBox_GetLBTextLen(d->model_combo, sel);
        std::wstring w((size_t)len, 0);
        ComboBox_GetLBText(d->model_combo, sel, &w[0]);
        return sl::w2a(w);
    }
    return "llama3.2:latest";
}

static bool on_cmd(Page* p, int id, HWND src) {
    AIData* d = A(p);
    switch (id) {
        case ID_AI_TOGGLE: {
            d->quantum = !d->quantum;
            SetCtrl(d->quantum_btn, d->quantum ? "🔮 Квантовый: ВКЛ" : "🧪 Квантовый режим");
            return true;
        }
        case ID_AI_SCAN: {
            sl_thread([p, d]() {
                std::string base = GetCtrl(d->endpoint);
                auto models = sl::ai::list_models(base);
                std::vector<std::string>* list = new std::vector<std::string>(models);
                sl_ai_set_models(d->model_combo, list);
                PageEvent* e = new PageEvent;
                e->kind = PE_AI; e->page = p->hwnd;
                if (models.empty()) e->a = "Модели не найдены. Запусти `ollama pull llama3.2`";
                else {
                    std::string joined;
                    for (size_t i = 0; i < models.size() && i < 5; i++) {
                        if (i) joined += ", ";
                        joined += models[i];
                    }
                    e->a = "Найдено моделей: " + joined + "...";
                }
                post_event(p->app->hwnd, e);
            });
            return true;
        }
        case ID_AI_SEND: {
            if (d->busy) return true;
            std::string msg = GetCtrl(d->input);
            if (msg.empty()) return true;
            SetCtrl(d->input, "");
            sl_memo_append(d->chat, "👤 Вы: " + msg);
            sl_memo_append(d->chat, "⚡ Думаю...");
            d->busy = true;
            sl_thread([p, d, msg]() {
                std::string base = GetCtrl(d->endpoint);
                std::string url = base + "/chat/completions";
                std::string prompt = msg;
                if (d->quantum) {
                    // квантовое преобразование
                    std::string pre[] = { "Представь что ты квантовый компьютер и ", "Квантово: ", "⚛ " };
                    std::string post[] = { " в квантовом состоянии", " ⚛", "" };
                    prompt = pre[0] + msg + post[0];
                }
                std::string err;
                std::string reply = sl::ai::chat_completions(url, d->api_key,
                                                             current_model(d), prompt, err);
                PageEvent* e = new PageEvent;
                e->kind = PE_AI; e->page = p->hwnd;
                if (reply.empty()) e->a = "Ошибка: " + (err.empty() ? "нет ответа" : err);
                else e->a = "🧠 AI: " + reply.substr(0, 500);
                post_event(p->app->hwnd, e);
            });
            return true;
        }
    }
    return false;
}

static void on_event(Page* p, PageEvent* ev) {
    AIData* d = A(p);
    switch (ev->kind) {
        case PE_AI:
            sl_memo_append(d->chat, ev->a);
            d->busy = false;
            break;
    }
}

HWND create_ai_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    AIData* d = new AIData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_event = on_event;

    sl::Config cfg;
    cfg.load();
    d->base_url = "http://localhost:11434/v1";
    d->api_key = "";

    int x = 40, y = 30, cw = 680;
    MakeLabel(h, 0, "AI Агент", x, y, cw, 30);
    y += 44;

    d->quantum_btn = MakeButton(h, ID_AI_TOGGLE, "🧪 Квантовый режим", x, y, 200, 34);
    y += 46;

    MakeLabel(h, 0, "Endpoint:", x, y, 100, 24);
    d->endpoint = MakeEdit(h, ID_AI_ENDPOINT, d->base_url, x + 100, y - 3, 360, 26);
    y += 40;

    MakeLabel(h, 0, "Модель:", x, y, 100, 24);
    d->model_combo = MakeCombo(h, ID_AI_MODEL, x + 100, y - 3, 360, 200);
    sl_fill_combo(d->model_combo, { "llama3.2:latest", "mistral:latest",
                                    "codellama:latest", "qwen2.5:latest" });
    MakeButton(h, ID_AI_SCAN, "Scan", x + 480, y - 3, 70, 30);
    y += 48;

    d->chat = MakeMemo(h, ID_AI_CHAT, x, y, cw, 300);
    SetCtrl(d->chat, "⚡ AI готов. Ollama: " + d->base_url);
    y += 316;

    d->input = MakeEdit(h, ID_AI_INPUT, "", x, y - 3, cw - 70, 44);
    MakeButton(h, ID_AI_SEND, "➤", x + cw - 64, y - 3, 60, 44);
    return h;
}

} // namespace slui