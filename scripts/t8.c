#include <stdio.h>
#include <string.h>
extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);
extern unsigned int sl_dbg_h[5];
extern unsigned int sl_dbg_hs[40];
extern int sl_dbg_n;
int main(void){
    unsigned char out[20];
    static char big[100000];
    memset(big,'a',sizeof(big));
    sl_sha1((const unsigned char*)big, 56, out);
    printf("n=%d\n", sl_dbg_n);
    for(int s=0;s<sl_dbg_n;s++){
        printf("stage %d: %08x %08x %08x %08x %08x\n",
            s, sl_dbg_hs[s*5], sl_dbg_hs[s*5+1], sl_dbg_hs[s*5+2], sl_dbg_hs[s*5+3], sl_dbg_hs[s*5+4]);
    }
    printf("out: "); for(int i=0;i<20;i++) printf("%02x", out[i]); printf("\n");
    return 0;
}
