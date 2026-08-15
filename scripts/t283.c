#include <stdio.h>
int main(void){
    extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);
    unsigned char out[20];
    static char big[300];
    for(int i=0;i<283;i++) big[i]='a';
    sl_sha1(big,283,out);
    printf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x 59404688ade8da321e81f2cb2a2f2efba590ecda\n",out[0],out[1],out[2],out[3],out[4],out[5],out[6],out[7],out[8],out[9],out[10],out[11],out[12],out[13],out[14],out[15],out[16],out[17],out[18],out[19]);
    return 0;
}
