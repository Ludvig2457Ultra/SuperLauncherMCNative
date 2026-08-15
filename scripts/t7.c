#include <stdio.h>
#include <string.h>
extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);
int main(void){
    unsigned char out[20];
    unsigned char m[128];
    memset(m, 0, sizeof(m));
    memset(m, 'a', 56);
    m[56] = 0x80;
    m[126]=0x01; m[127]=0xC0;   /* length 448 bits, big-endian */
    sl_sha1(m, 128, out);
    printf("padded128: "); for(int i=0;i<20;i++) printf("%02x", out[i]); printf("\n");
    return 0;
}
