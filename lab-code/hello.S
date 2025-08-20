  .text
  .globl main
main:
  addi sp, sp, -16
  li t0, 10
  sw t0, 0(sp)
  lw t1, 0(sp)
  sw t1, 4(sp)
  lw t2, 4(sp)
  li t3, 1
  add t4, t2, t3
  sw t4, 8(sp)
  lw t5, 8(sp)
  sw t5, 0(sp)
  lw t6, 0(sp)
  sw t6, 12(sp)
  lw a0, 12(sp)
  ret
  addi sp, sp, 16
