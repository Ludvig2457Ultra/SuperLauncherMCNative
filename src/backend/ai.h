#pragma once
#include <string>
#include <vector>

namespace sl {

namespace ai {

// GET {base}/api/tags — список имён моделей. base без "/v1".
std::vector<std::string> list_models(const std::string& base_url);

// POST {url}/chat/completions (OpenAI-совместимый). Возвращает текст ответа.
// url — полный адрес, например http://localhost:11434/v1/chat/completions.
// api_key — необязательный Bearer-токен.
// err — текст ошибки при неудаче.
std::string chat_completions(const std::string& url, const std::string& api_key,
                             const std::string& model, const std::string& user_message,
                             std::string& err, int timeout_seconds = 120);

} // namespace ai
} // namespace sl