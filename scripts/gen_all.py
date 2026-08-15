import hashlib
lines = ['#include <stdio.h>', '#include <string.h>',
         'extern void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);',
         'int main(void){',
         '    unsigned char out[20];',
         '    static char big[200000];',
         '    memset(big,0x61,sizeof(big));']
lens = list(range(0, 300)) + [511, 512, 513, 1000, 4096, 10000, 100000]
hexf = ','.join(['out[%d]' % i for i in range(20)])
for L in lens:
    e = hashlib.sha1(b'a' * L).hexdigest()
    fmt = 'printf("a%d ", %d); sl_sha1(big,%d,out); printf("%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x%%02x %s\\n",%s);'
    lines.append(fmt % (L, L, L, repr(e).strip("'"), hexf))
lines.append('    return 0;')
lines.append('}')
with open('scripts/gen_all.c', 'w', encoding='ascii') as f:
    f.write('\n'.join(lines))
print('wrote', len(lens), 'cases')