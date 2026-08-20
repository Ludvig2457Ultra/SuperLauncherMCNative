#include "account.h"
#include "../core/json.h"
#include "../core/paths.h"
#include "../core/log.h"
#include "../crypto/sha256.h"
#include <ctime>
#include <random>
#include <cstdio>

namespace sl {
namespace account {

using namespace sl::json;

static std::string pick(const std::string& rel) {
    const std::string root = app_root_utf8();
    std::string a = path_join(root, rel);
    if (file_exists(a)) return a;
    if (file_exists(rel)) return rel;
    return a;
}

static std::string accounts_file() { return pick("accounts.json"); }
static std::string licenses_file() { return pick("licenses.json"); }

static std::string now_iso() {
    const time_t t = time(nullptr);
    tm tm_;
    localtime_s(&tm_, &t);
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_);
    return buf;
}

static std::string hex_token(size_t n) {
    std::random_device rd;
    std::string out;
    const char* hx = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) out += hx[rd() % 16];
    return out;
}

// ---------------- сериализация ----------------
static Value user_to_json(const User& u) {
    Value v(Type::Object);
    v.obj = new std::vector<std::pair<std::string, Value>>();

    auto put = [&](const char* k, const std::string& s) {
        Value nv(Type::String); nv.str = s; v.obj->push_back({ k, std::move(nv) });
    };

    auto puti = [&](const char* k, const long long x) {
        Value nv(Type::Number); nv.num = static_cast<double>(x); v.obj->push_back({ k, std::move(nv) });
    };

    put("user_id", u.user_id); put("username", u.username); put("email", u.email);
    put("password_hash", u.password_hash);
    put("license_tier", u.license_tier); puti("level", u.level); puti("xp", u.xp);
    put("created_at", u.created_at); put("last_login", u.last_login);

    Value skins_arr(Type::Array); skins_arr.arr = new std::vector<Value>();
    for (auto& s : u.skins) {
        Value sv(Type::String); sv.str = s; skins_arr.arr->push_back(std::move(sv));
    }

    Value cust_arr(Type::Array); cust_arr.arr = new std::vector<Value>();
    for (auto& s : u.custom_skins) {
        Value sv(Type::String); sv.str = s; cust_arr.arr->push_back(std::move(sv));
    }

    v.obj->push_back({ "skins", std::move(skins_arr) });
    v.obj->push_back({ "custom_skins", std::move(cust_arr) });

    return v;
}

static User user_from_json(const Value& v) {
    User u;

    auto gs = [&](const char* k, std::string& out) {
        const Value* x = v.get(k);
        if (x && x->is_str()) out = x->as_string();
    };

    auto gi = [&](const char* k, long long& out) {
        const Value* x = v.get(k);
        if (x && x->is_num()) out = x->as_long();
    };

    gs("user_id", u.user_id); gs("username", u.username); gs("email", u.email);
    gs("password_hash", u.password_hash);
    gs("license_tier", u.license_tier); gi("level", (long long&)u.level); gi("xp", u.xp);
    gs("created_at", u.created_at); gs("last_login", u.last_login);

    if (const Value* x = v.get("skins"); x && x->is_arr())
        for (size_t i = 0; i < x->size(); i++)
            if (x->at(i).is_str()) u.skins.push_back(x->at(i).as_string());
    if (const Value* x = v.get("custom_skins"); x && x->is_arr())
        for (size_t i = 0; i < x->size(); i++)
            if (x->at(i).is_str()) u.custom_skins.push_back(x->at(i).as_string());

    return u;
}

std::vector<User> load_users() {
    std::vector<User> out;
    std::string path = accounts_file();
    if (!file_exists(path)) return out;
    std::string text = read_file_text(path);
    if (text.empty()) return out;
    Value root;
    if (!parse(text, root) || !root.is_arr()) return out;
    for (size_t i = 0; i < root.size(); i++) out.push_back(user_from_json(root.at(i)));
    return out;
}

void save_users(const std::vector<User>& users) {
    Value arr(Type::Array);
    arr.arr = new std::vector<Value>();
    for (auto& u : users) arr.arr->push_back(user_to_json(u));
    write_file_text(accounts_file(), dump(arr, 2));
}

std::vector<std::pair<std::string, License>> load_licenses() {
    std::vector<std::pair<std::string, License>> out;
    const std::string path = licenses_file();
    if (!file_exists(path)) return out;
    const std::string text = read_file_text(path);
    if (text.empty()) return out;
    Value root;
    if (!parse(text, root) || !root.is_obj()) return out;
    for (auto& kv : *root.obj) {
        License l;
        const Value& v = kv.second;
        if (const Value* x = v.get("tier"); x && x->is_str()) l.tier = x->as_string();
        if (const Value* x = v.get("expires_at"); x && x->is_num()) l.expires_at = x->as_long();
        if (const Value* x = v.get("activated"); x && x->is_bool()) l.activated = x->as_bool();
        if (const Value* x = v.get("activated_by"); x && x->is_str()) l.activated_by = x->as_string();
        out.push_back({ kv.first, l });
    }
    return out;
}

void save_licenses(const std::vector<std::pair<std::string, License>>& licenses) {
    Value root(Type::Object);
    root.obj = new std::vector<std::pair<std::string, Value>>();
    for (auto& kv : licenses) {
        Value v(Type::Object);
        v.obj = new std::vector<std::pair<std::string, Value>>();
        Value t(Type::String); t.str = kv.second.tier; v.obj->push_back({ "tier", std::move(t) });
        Value e(Type::Number); e.num = (double)kv.second.expires_at; v.obj->push_back({ "expires_at", std::move(e) });
        Value a(Type::Bool); a.b = kv.second.activated; v.obj->push_back({ "activated", std::move(a) });
        Value ab(Type::String); ab.str = kv.second.activated_by; v.obj->push_back({ "activated_by", std::move(ab) });
        root.obj->push_back({ kv.first, std::move(v) });
    }
    write_file_text(licenses_file(), dump(root, 2));
}

// ---------------- текущий пользователь ----------------
static std::unique_ptr<User> g_current = nullptr;
User* current() { return g_current.get(); }
void set_current(std::unique_ptr<User> u) {
    g_current = std::move(u);
}
void logout() { set_current(nullptr); }

void ensure_user_dir(const std::string& user_id) {
    const std::string dir = path_join(path_join(app_root_utf8(), "user_data"), user_id);
    mkdirs(dir);
}

// ---------------- регистрация / логин ----------------
bool register_user(const std::string& username, const std::string& email,
                   const std::string& password, User& out, std::string& err) {
    auto users = load_users();
    for (auto& u : users) {
        if (u.username == username) { err = "Имя занято"; return false; }
        if (!email.empty() && u.email == email) { err = "Email занят"; return false; }
    }
    User u;
    u.user_id = hex_token(16);
    u.username = username;
    u.email = email;
    u.license_tier = "free";
    u.level = 1;
    u.xp = 0;
    u.skins = {"default"};
    u.created_at = now_iso();
    u.last_login = now_iso();
    std::string salt = hex_token(8);
    u.password_hash = sha256_hex(password + salt) + ":" + salt;
    users.push_back(u);
    save_users(users);
    ensure_user_dir(u.user_id);
    out = u;
    set_current(std::make_unique<User>(User(u)));
    return true;
}

bool login_user(const std::string& username_or_email, const std::string& password,
                User& out, std::string& err) {
    auto users = load_users();
    for (auto& u : users) {
        const bool name_match = (u.username == username_or_email);
        const bool email_match = !u.email.empty() && u.email == username_or_email;
        if (name_match || email_match) {
            // проверка пароля: hash:salt
            const size_t colon = u.password_hash.rfind(':');
            if (colon == std::string::npos) { err = "Неверный логин или пароль"; return false; }
            const std::string stored = u.password_hash.substr(0, colon);
            const std::string salt = u.password_hash.substr(colon + 1);
            if (sha256_hex(password + salt) != stored) {
                err = "Неверный логин или пароль";
                return false;
            }
            out = u;
            out.last_login = now_iso();
            set_current(std::make_unique<User>(User(out)));
            for (auto& u2 : users) {
                if (u2.user_id == u.user_id) { u2.last_login = out.last_login; break; }
            }
            save_users(users);
            return true;
        }
    }
    err = "Неверный логин или пароль";
    return false;
}

bool activate_license(const std::string& key, const std::string& user_id,
                      std::string& msg) {
    if (key.empty()) { msg = "Введите ключ"; return false; }
    auto licenses = load_licenses();
    auto it = licenses.end();
    for (auto i = licenses.begin(); i != licenses.end(); ++i) {
        if (i->first == key) { it = i; break; }
    }
    if (it == licenses.end()) { msg = "Неверный ключ"; return false; }
    if (it->second.activated) { msg = "Уже активирована"; return false; }
    if (it->second.expires_at > 0 && it->second.expires_at < time(nullptr)) {
        msg = "Срок истек"; return false;
    }
    it->second.activated = true;
    it->second.activated_by = user_id;
    save_licenses(licenses);
    // обновить tier у пользователя
    auto users = load_users();
    for (auto& u : users) {
        if (u.user_id == user_id) { u.license_tier = it->second.tier; break; }
    }
    save_users(users);
    if (User* c = current(); c && c->user_id == user_id) c->license_tier = it->second.tier;
    msg = "Лицензия активирована!";
    return true;
}

} // namespace account

// ---------------- скины ----------------
namespace skins {

const std::vector<SkinInfo>& library() {
    static const std::vector<SkinInfo> lib = {
        { "default", "Стандартный", 0, "👤" },
        { "santa_hat", "Шапка Санты", 0, "🎅" },
        { "santa_suit", "Костюм Санты", 500, "🎄" },
        { "new_year_2026", "2026 Новый Год", 1000, "🎆" },
        { "reindeer", "Олень Рудольф", 300, "🦌" },
        { "snowman", "Снеговик", 250, "⛄" },
    };
    return lib;
}

static std::string normalize_skin_id(const std::string& id) {
    std::string result = id;

    const size_t start = result.find_first_not_of(" \t\n\r");
    const size_t end = result.find_last_not_of(" \t\n\r");
    result = (start == std::string::npos) ? "" : result.substr(start, end - start + 1);

    std::transform(result.begin(), result.end(), result.begin(),
                   [](const unsigned char c){ return std::tolower(c); });

    return result;
}

bool is_unlocked(const std::string& skin_id, const User& user) {
    const std::string normalized = normalize_skin_id(skin_id);
    if (normalized == "default") return true;

    std::vector<std::string> normalized_skins;
    normalized_skins.reserve(user.skins.size() + user.custom_skins.size());

    for (const auto& s : user.skins) {
        normalized_skins.push_back(normalize_skin_id(s));
    }
    for (const auto& s : user.custom_skins) {
        normalized_skins.push_back(normalize_skin_id(s));
    }

    return std::find(normalized_skins.begin(), normalized_skins.end(), normalized)
           != normalized_skins.end();
}

bool unlock(const std::string& skin_id, User& user, std::string& msg) {
    const SkinInfo* found = nullptr;
    for (auto& s : library()) if (s.id == skin_id) { found = &s; break; }
    if (!found) { msg = "Не найден"; return false; }
    if (is_unlocked(skin_id, user)) { msg = "Уже разблокирован"; return false; }
    if (user.xp < found->price) { msg = "Нужно " + std::to_string(found->price) + " XP"; return false; }
    user.xp -= found->price;
    user.skins.push_back(skin_id);
    // сохранить
    auto users = account::load_users();
    for (auto& u : users) {
        if (u.user_id == user.user_id) { u = user; break; }
    }
    account::save_users(users);
    msg = "Скин '" + found->name + "' разблокирован!";
    return true;
}

bool upload_custom(const std::string& image_path, User& user, std::string& out_id,
                   std::string& msg) {
    if (!file_exists(image_path)) { msg = "Файл не найден"; return false; }
    // проверка размера PNG 64x64 / 64x32 — читаем только если это PNG
    std::string bytes = read_file_text(image_path);
    if (bytes.size() >= 24 &&
        static_cast<unsigned char>(bytes[0]) == 0x89 &&
        bytes[1] == 'P' &&
        bytes[2] == 'N' &&
        bytes[3] == 'G') {

        auto read_be_u32 = [&](const size_t offset) -> uint32_t {
            return (static_cast<uint32_t>(bytes[offset]) << 24) |
                   (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
                   (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
                   static_cast<uint32_t>(bytes[offset + 3]);
        };

        const uint32_t w = read_be_u32(16);
        const uint32_t h = read_be_u32(20);

        if (!((w == 64 && h == 64) || (w == 64 && h == 32))) {
            msg = "Размер должен быть 64x64 или 64x32";
            return false;
        }
    }
    const std::string id = "custom_" + account::hex_token(6);
    const std::string dir = path_join(path_join(app_root_utf8(),
        path_join("user_data", user.user_id)), "skins");
    mkdirs(dir);
    const std::string dst = path_join(dir, id + ".png");
    if (!copy_file(image_path, dst)) { msg = "Ошибка копирования"; return false; }
    user.custom_skins.push_back(id);
    auto users = account::load_users();
    for (auto& u : users) {
        if (u.user_id == user.user_id) { u = user; break; }
    }
    account::save_users(users);
    out_id = id;
    msg = "Скин загружен!";
    return true;
}

} // namespace skins
} // namespace sl