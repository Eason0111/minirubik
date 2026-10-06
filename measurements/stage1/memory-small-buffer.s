.text
main:
    li t0, 0x20000000
    li t1, 256

write_loop:
    sw zero, 0(t0)
    addi t0, t0, 4
    addi t1, t1, -1
    bne t1, zero, write_loop

hold:
    j hold
