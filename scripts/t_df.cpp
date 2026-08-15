#include "../src/core/zip.h"
#include "../src/core/paths.h"
#include <cstdio>
using namespace sl;
int main() {
    mkdirs("build_t/dfout");
    bool z = is_zip("build_t/df.zip");
    printf("is_zip=%d\n", z);
    int n = zip_extract_all("build_t/df.zip", "build_t/dfout");
    printf("extracted=%d\n", n);
    printf("content=[%s]\n", read_file_text("build_t/dfout/hello.txt").c_str());
    return n == 1 ? 0 : 1;
}