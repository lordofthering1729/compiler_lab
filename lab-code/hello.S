  .text
  .globl main
main:
  li    t0, 3
  xor   t0, t0, 0
  seqz  t0, t0
  mv    t1, t0
  xor   t1, t1, 0
  snez  t1, t1
  li    t2, 4
  xor   t2, t2, 0
  snez  t2, t2
  mv    t3, t1
  or    t3, t3, t2
  li    t4, 2
  mul   t4, t4, t3
  li    t5, 1
  add   t5, t5, t4
  mv    t6, t5
  li    t0, 5
  slt   t6, t6, t0
  mv    a0, t6
  ret
