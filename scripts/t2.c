#include <stdio.h>
#include <string.h>
extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);
int main(void){
    unsigned char out[20];
    static char buf[64];
    memset(buf, 'a', 64);
    printf("before\n"); fflush(stdout);
    sl_sha1((const unsigned char*)buf, 64, out);
    printf("after\n"); fflush(stdout);
    for(int i=0;i<20;i++) printf("%02x", out[i]);
    printf("\n");
    return 0;
}
