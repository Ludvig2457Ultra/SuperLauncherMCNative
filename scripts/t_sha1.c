#include <stdio.h>
#include <string.h>
extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);
static void hex(const unsigned char* b, int n){int i;for(i=0;i<n;i++)printf("%02x",b[i]);printf("\n");}
int main(void){
    unsigned char out[20];
    const char* cases[] = {"", "abc", "The quick brown fox jumps over the lazy dog", "abcdefghijklmnopqrstuvwxyz0123456789"};
    int i;
    for (i = 0; i < 4; i++) {
        printf("case %d len %d ... ", i, (int)strlen(cases[i])); fflush(stdout);
        sl_sha1((const unsigned char*)cases[i], strlen(cases[i]), out);
        printf("ok: "); hex(out, 20);
    }
    static char big[100000];
    memset(big, 'a', sizeof(big));
    int lens[] = {100000, 64, 100, 128, 55, 56, 57, 63, 1000, 4096};
    for (i = 0; i < 10; i++) {
        printf("big case %d len %d ... ", i, lens[i]); fflush(stdout);
        sl_sha1((const unsigned char*)big, lens[i], out);
        printf("ok: "); hex(out, 20);
    }
    return 0;
}