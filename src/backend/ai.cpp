#include "ai.h"
#include "../core/json.h"
#include "../net/http.h"

namespace sl {
namespace ai {

using namespace sl::json;

std::vector<std::string> list_models(const std::string& base_url) {
    std::vector<std::string> out;
    std::string url = base_url;
    if (url.empty()) return out;
    if (url.size() >= 3 && url.substr(url.size() - 3) == "/v1") url = url.substr(0, url.size() - 3);
    if (url.back() == '/') url.pop_back();
    url += "/api/tags";
    NetConfig c;
    c.timeout_seconds = 5;
    std::string body = http_get(url.c_str(), c);
    Value root;
    if (body.empty() || !parse(body, root)) return out;
    if (const Value* m = root.get("models"); m && m->is_arr()) {
        for (size_t i = 0; i < m->size(); i++) {
            const Value* n = m->at(i).get("name");
            if (n && n->is_str()) out.push_back(n->as_string());
        }
    }
    return out;
}

std::string chat_completions(const std::string& url, const std::string& api_key,
                             const std::string& model, const std::string& user_message,
                             std::string& err, int timeout_seconds) {
    Value payload(Type::Object);
    payload.obj = new std::vector<std::pair<std::string, Value>>();
    auto put = [&](const char* k, Value&& v) { payload.obj->push_back({ k, std::move(v) }); };
    Value m(Type::String); m.str = model; put("model", std::move(m));
    Value msgs(Type::Array);
    msgs.arr = new std::vector<Value>();
    Value msg(Type::Object);
    msg.obj = new std::vector<std::pair<std::string, Value>>();
    Value role(Type::String); role.str = "user"; msg.obj->push_back({ "role", std::move(role) });
    Value content(Type::String); content.str = user_message; msg.obj->push_back({ "content", std::move(content) });
    msgs.arr->push_back(std::move(msg));
    put("messages", std::move(msgs));
    Value mt(Type::Number); mt.num = 1024; put("max_tokens", std::move(mt));
    Value temp(Type::Number); temp.num = 0.7; put("temperature", std::move(temp));

    NetConfig c;
    c.timeout_seconds = timeout_seconds;
    std::vector<HttpHead> hd;
    if (!api_key.empty()) hd.push_back({ "Authorization", "Bearer " + api_key });

    std::string body = http_post_json(url.c_str(), dump(payload, 0), hd, c);
    if (body.empty()) {
        err = "Ollama не запущен. Запусти `ollama serve`";
        return "";
    }
    Value root;
    if (!parse(body, root)) {
        err = "Неверный ответ API";
        return "";
    }
    const Value* choices = root.get("choices");
    if (choices && choices->is_arr() && choices->size() > 0) {
        const Value* msg2 = choices->at(0).get("message");
        if (msg2 && msg2->is_obj()) {
            const Value* cc = msg2->get("content");
            if (cc && cc->is_str()) return cc->as_string();
        }
    }
    err = "Ошибка: нет ответа от модели";
    return "";
}

} // namespace ai
} // namespace sl