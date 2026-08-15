#include <stdio.h>
#include <string.h>
extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);
extern unsigned int sl_dbg_ws[320];
extern int sl_dbg_n;
int main(void){
    unsigned char out[20];
    static char big[100000];
    memset(big,'a',sizeof(big));
    sl_sha1((const unsigned char*)big, 56, out);
    printf("n=%d\n", sl_dbg_n);
    for(int s=0;s<sl_dbg_n;s++){
        printf("stage %d W0-15: ", s);
        for(int j=0;j<16;j++) printf("%08x ", sl_dbg_ws[s*80+j]);
        printf("\n");
    }
    return 0;
}
