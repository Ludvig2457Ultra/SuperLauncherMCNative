; ==========================================================================
;  sha1.asm - SHA-1 (FIPS 180-1), чистый x64 ассемблер для ML64 (MSVC).
;  Без зависимостей от CRT. Используется для проверки контрольных сумм
;  библиотек Minecraft (fix: "wrong Checksum get da39a3ee..." empty jars).
;
;  Экспорт (ABI Win64):
;    void sl_sha1(const unsigned char* data, unsigned long long len,
;                 unsigned char out[20]);
;      rcx = data, rdx = len, r8 = out
;
;  Раскладка кадра (rbp = rsp после выделения):
;    BLK   = rbp+0   (64 байта буфер последнего блока/паддинга)
;    W     = rbp+64  (80 x dword = 320 байт расписание сообщения)
;    H     = rbp+384 (5 x dword текущее состояние a..e)
;    PREV  = rbp+404 (5 x dword снимок H перед раундами)
;    кадр = 432, затем mov rbp,rsp
; ==========================================================================

.CODE

.CODE

; --------------------------------------------------------------------------
;  sl_sha1: главная процедура
; --------------------------------------------------------------------------
sl_sha1 PROC

    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 464
    mov     rbp, rsp

    mov     r13, rcx            ; data
    mov     r14, rdx            ; len (bytes)
    mov     r15, r8             ; out

    ; ---- инициализация H0..H4
    mov     dword ptr [rbp+384], 067452301h
    mov     dword ptr [rbp+388], 0EFCDAB89h
    mov     dword ptr [rbp+392], 098BADCFEh
    mov     dword ptr [rbp+396], 010325476h
    mov     dword ptr [rbp+400], 0C3D2E1F0h

    ; ---- основные полные блоки (len / 64)
    ; r13 = текущий указатель данных, r12 = счётчик оставшихся блоков
    mov     rax, r14
    shr     rax, 6
    test    rax, rax
    jz      remainder
    mov     r12, rax            ; счётчик блоков
block_loop:
    ; загрузить 64 байта из r13 в W (big-endian)
    lea     r11, [rbp+64]       ; W
    xor     ecx, ecx
load_w:
    mov     eax, dword ptr [r13 + rcx*4]
    bswap   eax
    mov     dword ptr [r11 + rcx*4], eax
    inc     ecx
    cmp     ecx, 16
    jl      load_w

    call    do_block

    add     r13, 64
    dec     r12
    jnz     block_loop

remainder:
    ; ---- последний блок с паддингом
    mov     r11, r14
    and     r11, 63             ; остаток

    ; обнулить BLK (rbp+0..63)
    push    rcx
    mov     ecx, 16
    xor     eax, eax
zfill1:
    mov     qword ptr [rbp + rcx*8 - 8], rax
    dec     ecx
    jnz     zfill1
    pop     rcx

    ; скопировать остаток (r11 байт) из r13 в BLK
    xor     ecx, ecx
copy_rem:
    cmp     ecx, r11d
    jge     copydone
    mov     al, byte ptr [r13 + rcx]
    mov     byte ptr [rbp + rcx], al
    inc     ecx
    jmp     copy_rem
copydone:

    ; битовая длина = len * 8
    mov     rdx, r14
    shl     rdx, 3
    mov     eax, edx            ; low dword
    shr     rdx, 32
    mov     ebx, edx            ; high dword
    bswap   eax
    bswap   ebx
    mov     dword ptr [rbp+432], eax   ; low BE
    mov     dword ptr [rbp+436], ebx   ; high BE

    ; 0x80 на позиции rem
    mov     byte ptr [rbp + r11], 80h

    ; помещается ли длина в этот блок: rem+1 <= 56 ?
    lea     r9d, [r11+1]
    cmp     r9d, 57
    jl      single_block

    ; ---- два блока
    ; первый блок: payload + 0x80 + нули (длина пойдёт во второй)
    lea     r11, [rbp+64]       ; W
    xor     ecx, ecx
load_w4:
    mov     eax, dword ptr [rbp + rcx*4]
    bswap   eax
    mov     dword ptr [r11 + rcx*4], eax
    inc     ecx
    cmp     ecx, 16
    jl      load_w4
    call    do_block

    ; второй блок: нули + длина
    mov     ecx, 16
    xor     eax, eax
zfill2:
    mov     qword ptr [rbp + rcx*8 - 8], rax
    dec     ecx
    jnz     zfill2
    mov     eax, dword ptr [rbp+432]
    mov     ebx, dword ptr [rbp+436]
    mov     dword ptr [rbp + 56], ebx
    mov     dword ptr [rbp + 60], eax
    lea     r11, [rbp+64]
    xor     ecx, ecx
load_w5:
    mov     eax, dword ptr [rbp + rcx*4]
    bswap   eax
    mov     dword ptr [r11 + rcx*4], eax
    inc     ecx
    cmp     ecx, 16
    jl      load_w5
    call    do_block
    jmp     write_out

single_block:
    mov     eax, dword ptr [rbp+432]
    mov     ebx, dword ptr [rbp+436]
    mov     dword ptr [rbp + 56], ebx
    mov     dword ptr [rbp + 60], eax
    ; переложить байты BLK (payload+0x80+нули+длина) в W big-endian
    lea     r11, [rbp+64]
    xor     ecx, ecx
load_w2:
    mov     eax, dword ptr [rbp + rcx*4]
    bswap   eax
    mov     dword ptr [r11 + rcx*4], eax
    inc     ecx
    cmp     ecx, 16
    jl      load_w2
    call    do_block

write_out:
    ; H -> out (20 байт, big-endian dwords)
    mov     eax, dword ptr [rbp+384]
    bswap   eax
    mov     dword ptr [r15], eax
    mov     eax, dword ptr [rbp+388]
    bswap   eax
    mov     dword ptr [r15+4], eax
    mov     eax, dword ptr [rbp+392]
    bswap   eax
    mov     dword ptr [r15+8], eax
    mov     eax, dword ptr [rbp+396]
    bswap   eax
    mov     dword ptr [r15+12], eax
    mov     eax, dword ptr [rbp+400]
    bswap   eax
    mov     dword ptr [r15+16], eax

    add     rsp, 464
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; --------------------------------------------------------------------------
;  do_block: обработать один блок (уже в W), обновить H. Вызывается через call.
; --------------------------------------------------------------------------
do_block:
    ; PREV = H
    mov     ecx, 5
prev_loop:
    mov     eax, dword ptr [rbp+384+rcx*4-4]
    mov     dword ptr [rbp+404+rcx*4-4], eax
    dec     ecx
    jnz     prev_loop

    ; расширить W до 80 слов (i = 16..79)
    mov     ecx, 16
ext_w:
    mov     eax, dword ptr [rbp+64+rcx*4-12]   ; w[i-3]
    xor     eax, dword ptr [rbp+64+rcx*4-32]   ; w[i-8]
    xor     eax, dword ptr [rbp+64+rcx*4-56]   ; w[i-14]
    xor     eax, dword ptr [rbp+64+rcx*4-64]   ; w[i-16]
    mov     edx, eax
    shl     eax, 1
    shr     edx, 31
    or      eax, edx            ; rol1
    mov     dword ptr [rbp+64+rcx*4], eax
    inc     ecx
    cmp     ecx, 80
    jl      ext_w

    ; ---- 80 раундов. a=eax b=edx c=esi d=edi e=r9d
    xor     ecx, ecx
round_loop:
    mov     eax,  dword ptr [rbp+384]   ; a
    mov     edx,  dword ptr [rbp+388]   ; b
    mov     esi,  dword ptr [rbp+392]   ; c
    mov     edi,  dword ptr [rbp+396]   ; d
    mov     r9d,  dword ptr [rbp+400]   ; e

    cmp     ecx, 20
    jl      do_f
    cmp     ecx, 40
    jl      do_g
    cmp     ecx, 60
    jl      do_h
    ; G, k4 (60..79)
    mov     r8d, edx
    xor     r8d, esi
    xor     r8d, edi                    ; f = b^c^d
    mov     r10d, eax
    shl     r10d, 5
    mov     r11d, eax
    shr     r11d, 27
    or      r10d, r11d                  ; rol5(a)
    add     r10d, r8d                   ; +f
    add     r10d, r9d                   ; +e
    add     r10d, 0CA62C1D6h            ; +k
    add     r10d, dword ptr [rbp+64+rcx*4]   ; +w[i]
    jmp     store_state
do_f:
    mov     r8d, edx
    and     r8d, esi                    ; b&c
    mov     r10d, edx
    not     r10d
    and     r10d, edi                   ; ~b&d
    or      r8d, r10d                   ; f = (b&c)|(~b&d)
    mov     r10d, eax
    shl     r10d, 5
    mov     r11d, eax
    shr     r11d, 27
    or      r10d, r11d
    add     r10d, r8d
    add     r10d, r9d
    add     r10d, 05A827999h
    add     r10d, dword ptr [rbp+64+rcx*4]
    jmp     store_state
do_g:
    mov     r8d, edx
    xor     r8d, esi
    xor     r8d, edi                    ; f = b^c^d
    mov     r10d, eax
    shl     r10d, 5
    mov     r11d, eax
    shr     r11d, 27
    or      r10d, r11d
    add     r10d, r8d
    add     r10d, r9d
    add     r10d, 06ED9EBA1h
    add     r10d, dword ptr [rbp+64+rcx*4]
    jmp     store_state
do_h:
    mov     r8d, edx
    and     r8d, esi                    ; b&c
    mov     r10d, edx
    and     r10d, edi                   ; b&d
    or      r8d, r10d
    mov     r10d, esi
    and     r10d, edi                   ; c&d
    or      r8d, r10d                   ; f = (b&c)|(b&d)|(c&d)
    mov     r10d, eax
    shl     r10d, 5
    mov     r11d, eax
    shr     r11d, 27
    or      r10d, r11d
    add     r10d, r8d
    add     r10d, r9d
    add     r10d, 08F1BBCDCh
    add     r10d, dword ptr [rbp+64+rcx*4]
store_state:
    mov     dword ptr [rbp+400], edi    ; e = старое d
    mov     dword ptr [rbp+396], esi    ; d = старое c
    mov     r11d, edx
    shl     edx, 30
    shr     r11d, 2
    or      edx, r11d                   ; rol30(b)
    mov     dword ptr [rbp+392], edx    ; c = rol30(б)
    mov     dword ptr [rbp+388], eax    ; b = старое a
    mov     dword ptr [rbp+384], r10d   ; a = t
    inc     ecx
    cmp     ecx, 80
    jl      round_loop

    ; ---- H += PREV
    mov     ecx, 5
add_loop:
    mov     eax, dword ptr [rbp+384+rcx*4-4]
    add     eax, dword ptr [rbp+404+rcx*4-4]
    mov     dword ptr [rbp+384+rcx*4-4], eax
    dec     ecx
    jnz     add_loop

    ret

sl_sha1 ENDP

END