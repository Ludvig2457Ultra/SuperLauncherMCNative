// Интеграционный смоук-тест для модулей нативных портов SuperLauncher.
#include "../src/core/common.h"
#include "../src/core/paths.h"
#include "../src/core/log.h"
#include "../src/core/config.h"
#include "../src/core/zip.h"
#include "../src/crypto/sha1file.h"
#include "../src/minecraft/version.h"
#include "../src/minecraft/install.h"
#include "../src/instances/instances.h"
#include "../src/net/http.h"
#include <cstring>

using namespace sl;

static int fails = 0, passes = 0;
#define CHECK(cond, msg) do { if (cond) { passes++; printf("PASS  %s\n", msg); } else { fails++; printf("FAIL  %s\n", msg); } } while (0)

int test_json();
int test_sha1();
int test_zip();
int test_paths();
int test_instances();
int test_manifest();

int main() {
    log_set_file("build_t/test.log");
    printf("== SuperLauncher native smoke test ==\n");
    test_json();
    test_sha1();
    test_zip();
    test_paths();
    test_instances();
    test_manifest();
    printf("\nPASS=%d FAIL=%d\n", passes, fails);
    return fails ? 1 : 0;
}

int test_json() {
    printf("-- json --\n");
    using namespace sl::json;
    const char* sample = R"({"versions":[{"id":"1.21.1","type":"release"}],"meta":{"sha1":"abc123","size":123}})";
    Value root;
    std::string err;
    bool ok = parse(sample, root, &err);
    CHECK(ok, "json parse ok");
    if (ok) {
        CHECK(root.get("versions") && root.get("versions")->is_arr(), "json versions array");
        CHECK(root.get("meta")->get("sha1")->as_string() == "abc123", "json nested string");
        CHECK(root.get("meta")->get("size")->as_long() == 123, "json nested number");
    }
    return 0;
}

int test_sha1() {
    printf("-- sha1 --\n");
    CHECK(sha1_hex((const unsigned char*)"abc", 3) == "a9993e364706816aba3e25717850c26c9cd0d89d", "sha1 abc");
    CHECK(sha1_hex((const unsigned char*)"", 0) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "sha1 empty");
    CHECK(sha1_hex((const unsigned char*)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56) ==
          "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "sha1 56 block2 edge");
    // файл
    write_file_text("build_t/t.txt", "hello");
    CHECK(sha1_file("build_t/t.txt") == "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d", "sha1 file hello");
    return 0;
}

int test_zip() {
    printf("-- zip --\n");
    // строим минимальный zip со STORED-файлом "x.txt" = "hi"
    // [local header 30B][name 5B][data 2B][central 46B][name 5B][eocd 22B]
    std::vector<unsigned char> z;
    auto add16 = [&](unsigned v) { z.push_back((unsigned char)(v & 0xFF)); z.push_back((unsigned char)((v >> 8) & 0xFF)); };
    auto add32 = [&](unsigned v) { for (int i = 0; i < 4; i++) z.push_back((unsigned char)((v >> (8 * i)) & 0xFF)); };
    const char* name = "x.txt";
    auto push_local = [&](const char* nm) {
        z.insert(z.end(), {'P','K',3,4});
        add16(20); add16(0); add16(0); add16(0); add16(0); // ver flags method time date
        add32(0); add32(2); add32(2); add16((unsigned)strlen(nm)); add16(0);
    };
    push_local(name);
    z.insert(z.end(), name, name + 5);
    z.insert(z.end(), {'h','i'});
    unsigned cd_off = (unsigned)z.size();
    z.insert(z.end(), {'P','K',1,2});
    add16(20); add16(20); add16(0); add16(0); add16(0); add16(0); // vM vN flags method time date
    add32(0); add32(2); add32(2); add16(5); add16(0); add16(0);
    add16(0); add16(0); add32(0); add32(0); // external attrs, local offset = 0
    z.insert(z.end(), name, name + 5);
    unsigned cd_size = (unsigned)z.size() - cd_off;
    z.insert(z.end(), {'P','K',5,6}); add16(0); add16(0); add16(1); add16(1);
    add32(cd_size); add32(cd_off); add16(0);

    FILE* f = fopen("build_t/stored.zip", "wb");
    fwrite(z.data(), 1, z.size(), f);
    fclose(f);
    CHECK(is_zip("build_t/stored.zip"), "zip magic");
    int n = zip_extract_all("build_t/stored.zip", "build_t/zipout");
    CHECK(n == 1, "zip extract count");
    CHECK(read_file_text("build_t/zipout/x.txt") == "hi", "zip extract content");
    return 0;
}

int test_paths() {
    printf("-- paths --\n");
    std::string mc = minecraft_directory();
    CHECK(!mc.empty() && mc.find(".minecraft") != std::string::npos, "minecraft dir");
    CHECK(file_exists("src/core/json.h"), "relative file exists");
    CHECK(parent_dir("a/b/c.txt") == "a/b", "parent_dir");
    CHECK(file_name("a/b/c.txt") == "c.txt", "file_name");
    return 0;
}

int test_instances() {
    printf("-- instances --\n");
    auto list = InstancesManager::all();
    Instance i = InstancesManager::create("SmokeTest", "1.21.1", "Vanilla", "Minecraft");
    CHECK(!i.id.empty(), "instance created with id");
    auto list2 = InstancesManager::all();
    bool found = false;
    for (auto& x : list2) if (x.id == i.id) found = true;
    CHECK(found, "instance persisted");
    InstancesManager::remove(i.id);
    return 0;
}

int test_manifest() {
    printf("-- manifest (network) --\n");
    std::vector<ManifestVersion> v;
    std::string err;
    bool ok = fetch_manifest(v, &err);
    if (!ok) {
        printf("  (net unavailable: %s)\n", err.c_str());
        return 0;
    }
    CHECK(!v.empty(), "manifest non-empty");
    bool hasrel = false;
    for (auto& x : v) if (x.type == "release") hasrel = true;
    CHECK(hasrel, "manifest has release");
    return 0;
}