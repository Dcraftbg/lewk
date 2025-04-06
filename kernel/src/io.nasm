[BITS 64]
section .text
extern outb
extern inb
; di port
; sil val
outb:
    mov al, sil
    xchg dx, di
    out dx, al
    xchg dx, di
    ret
; di port
inb:
    xchg dx, di
    in al, dx
    xchg dx, di
    ret
