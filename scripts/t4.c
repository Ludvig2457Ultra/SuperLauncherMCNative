#include <stdio.h>
#include <string.h>
extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);
extern unsigned int sl_dbg_w[80];
extern unsigned int sl_dbg_h[5];
void dump(void){
    printf("W0-3: %08x %08x %08x %08x\n", sl_dbg_w[0],sl_dbg_w[1],sl_dbg_w[2],sl_dbg_w[3]);
    printf("H   : %08x %08x %08x %08x %08x\n", sl_dbg_h[0],sl_dbg_h[1],sl_dbg_h[2],sl_dbg_h[3],sl_dbg_h[4]);
}
int main(void){
    unsigned char out[20];
    sl_sha1((const unsigned char*)"", 0, out);
    dump();
    printf("out: "); for(int i=0;i<20;i++) printf("%02x", out[i]); printf("\n");
    return 0;
}
