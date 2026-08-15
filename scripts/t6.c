#include <stdio.h>
#include <string.h>
extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);
int main(void){
    unsigned char out[20];
    static char big[100000];
    memset(big,'a',sizeof(big));
    int lens[]={56,57,63,64};
    for(int i=0;i<4;i++){
        printf("len %d ... ", lens[i]); fflush(stdout);
        sl_sha1((const unsigned char*)big, lens[i], out);
        printf("ok\n"); fflush(stdout);
    }
    return 0;
}
