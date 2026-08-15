#include "updates.h"
#include "../core/json.h"
#include "../core/log.h"
#include "../net/http.h"

namespace sl {
namespace updates {

using namespace sl::json;

bool fetch_releases(std::vector<ReleaseInfo>& out) {
    NetConfig nc;
    nc.user_agent = "SuperLauncher/2.0";
    std::string body = http_get(
        "https://api.github.com/repos/Ludvig2457Ultra/SuperLauncherMC/releases?per_page=10",
        nc);
    if (body.empty()) return false;
    Value root;
    if (!parse(body, root) || !root.is_arr()) return false;
    out.clear();
    for (size_t i = 0; i < root.size(); i++) {
        const Value& r = root.at(i);
        ReleaseInfo ri;
        if (const Value* x = r.get("tag_name"); x && x->is_str()) ri.tag = x->as_string();
        if (const Value* x = r.get("name"); x && x->is_str()) ri.name = x->as_string();
        if (const Value* x = r.get("body"); x && x->is_str()) ri.body = x->as_string();
        if (const Value* x = r.get("published_at"); x && x->is_str()) {
            ri.date = x->as_string();
            if (ri.date.size() > 10) ri.date = ri.date.substr(0, 10);
        }
        if (const Value* x = r.get("prerelease"); x && x->is_bool()) ri.prerelease = x->as_bool();
        // первый asset с .exe или .py
        ri.dl_url = "";
        if (const Value* a = r.get("assets"); a && a->is_arr()) {
            for (size_t j = 0; j < a->size() && ri.dl_url.empty(); j++) {
                const Value* f = a->at(j).get("browser_download_url");
                if (f && f->is_str()) {
                    std::string u = f->as_string();
                    bool is_exe = u.size() > 4 && u.substr(u.size() - 4) == ".exe";
                    bool is_py = u.size() > 3 && u.substr(u.size() - 3) == ".py";
                    if (is_exe || is_py) ri.dl_url = u;
                }
            }
            if (ri.dl_url.empty() && a->size() > 0) {
                const Value* f = a->at(0).get("browser_download_url");
                if (f && f->is_str()) ri.dl_url = f->as_string();
            }
        }
        out.push_back(ri);
    }
    return !out.empty();
}

int compare_versions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& v, std::vector<long long>& nums) {
        std::string cur;
        for (size_t i = 0; i < v.size(); i++) {
            char c = v[i];
            if (isdigit((unsigned char)c)) cur += c;
            else if (!cur.empty()) { nums.push_back(atoll(cur.c_str())); cur.clear(); }
        }
        if (!cur.empty()) nums.push_back(atoll(cur.c_str()));
    };
    std::vector<long long> na, nb;
    parts(a, na);
    parts(b, nb);
    size_t n = na.size() > nb.size() ? na.size() : nb.size();
    for (size_t i = 0; i < n; i++) {
        long long x = i < na.size() ? na[i] : 0;
        long long y = i < nb.size() ? nb[i] : 0;
        if (x != y) return x > y ? 1 : -1;
    }
    return 0;
}

bool check_for_update(const std::string& current_version, ReleaseInfo& out) {
    std::vector<ReleaseInfo> rels;
    if (!fetch_releases(rels)) return false;
    for (auto& r : rels) {
        if (compare_versions(r.tag, current_version) > 0) {
            out = r;
            return true;
        }
    }
    return false;
}

} // namespace updates
} // namespace sl