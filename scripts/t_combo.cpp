#include "src/minecraft/version.h"
#include "src/core/common.h"
#include "src/core/win.h"
#include <cstdio>
#include <fstream>
#include <vector>

int main() {
    std::ofstream out("build_t/combo_dbg.txt");
    std::vector<sl::ManifestVersion> list;
    std::string err;
    bool ok = sl::fetch_manifest(list, &err);
    out << "fetch_manifest=" << ok << " err=[" << err << "]\n";
    out << "count=" << list.size() << "\n";
    int shown = 0;
    for (auto& v : list) {
        if (shown < 40) out << "id=[" << v.id << "] type=[" << v.type << "]\n";
        shown++;
    }
    out << "--- first raw bytes of id[0] ---\n";
    if (!list.empty()) {
        for (unsigned char c : list[0].id) out << (int)c << ",";
        out << "\nlen hex=" << list[0].id.size() << "\n";
    }
    out.close();
    return 0;
}