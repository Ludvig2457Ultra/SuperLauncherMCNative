#pragma once
#include <string>
#include <vector>

namespace sl {

// Пользователь (зеркало backend/account.py + models/types.py).
struct User {
    std::string user_id;
    std::string username = "Guest";
    std::string email;
    std::string password_hash;   // sha256(password + salt) + ":" + salt
    std::string license_tier = "free";
    int level = 1;
    long long xp = 0;
    std::vector<std::string> skins = {"default"};
    std::vector<std::string> custom_skins;
    std::string created_at;
    std::string last_login;
};

// Скин из каталога библиотеки.
struct SkinInfo {
    std::string id;
    std::string name;
    long long price = 0;
    std::string icon;   // emoji
};

namespace account {

// Текущий пользователь (nullptr если не вошёл).
User* current();
void set_current(User* u) ; // принимает владение; удаляет старый

// Загрузка/сохранение accounts.json
std::vector<User> load_users();
void save_users(const std::vector<User>& users);
// licenses.json: key -> {"tier", "expires_at", "activated", "activated_by"}
struct License {
    std::string tier = "standard";
    long long expires_at = 0; // unixtime, 0 = вечная
    bool activated = false;
    std::string activated_by;
};
std::vector<std::pair<std::string, License>> load_licenses();
void save_licenses(const std::vector<std::pair<std::string, License>>& licenses);

// Регистрация. Возвращает true и заполняет user при успехе.
bool register_user(const std::string& username, const std::string& email,
                   const std::string& password, User& out, std::string& err);
// Логин по username ИЛИ email.
bool login_user(const std::string& username_or_email, const std::string& password,
                User& out, std::string& err);
void logout();

// Активация лицензии ключом.
bool activate_license(const std::string& key, const std::string& user_id,
                      std::string& msg);

// Создать каталог пользователя user_data/<id>
void ensure_user_dir(const std::string& user_id);

} // namespace account

// ---------------- Скины ----------------
namespace skins {

// Библиотека скинов (id, имя, цена, иконка).
const std::vector<SkinInfo>& library();

bool is_unlocked(const std::string& skin_id, const User& user);
// Покупка скина за XP. Возвращает true + сообщение.
bool unlock(const std::string& skin_id, User& user, std::string& msg);
// Загрузка кастомного скина из файла. Ошибки по образцу Python.
bool upload_custom(const std::string& image_path, User& user, std::string& out_id,
                   std::string& msg);

} // namespace skins

} // namespace sl