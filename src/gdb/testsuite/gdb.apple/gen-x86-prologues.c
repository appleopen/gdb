// NOTE NOTE NOTE NOTE  THIS IS A GENERATED FILE!!
// Any edits will be deleted!
// Instead, change gen-x86-prologues.rb and re-generate.
//


void pattern_1 (void);
void func_under_pattern_1 (void);
void pattern_2 (void);
void func_under_pattern_2 (void);
void pattern_3 (void);
void func_under_pattern_3 (void);
void pattern_4 (void);
void func_under_pattern_4 (void);
void pattern_5 (void);
void func_under_pattern_5 (void);
void pattern_6 (void);
void func_under_pattern_6 (void);
void pattern_7 (void);
void func_under_pattern_7 (void);
void pattern_8 (void);
void func_under_pattern_8 (void);
void pattern_9 (void);
void func_under_pattern_9 (void);
void pattern_10 (void);
void func_under_pattern_10 (void);
void pattern_11 (void);
void func_under_pattern_11 (void);
void pattern_12 (void);
void func_under_pattern_12 (void);
void pattern_13 (void);
void func_under_pattern_13 (void);
void pattern_14 (void);
void func_under_pattern_14 (void);
void pattern_15 (void);
void func_under_pattern_15 (void);
void pattern_16 (void);
void func_under_pattern_16 (void);
void pattern_17 (void);
void func_under_pattern_17 (void);
void pattern_18 (void);
void func_under_pattern_18 (void);
void pattern_19 (void);
void func_under_pattern_19 (void);
void pattern_20 (void);
void func_under_pattern_20 (void);
void pattern_21 (void);
void func_under_pattern_21 (void);
void pattern_22 (void);
void func_under_pattern_22 (void);
void pattern_23 (void);
void func_under_pattern_23 (void);
void pattern_24 (void);
void func_under_pattern_24 (void);
void pattern_25 (void);
void func_under_pattern_25 (void);

main (int argc, char **argv, char **envp)
{
  pattern_1 ();
  pattern_2 ();
  pattern_3 ();
  pattern_4 ();
  pattern_5 ();
  pattern_6 ();
  pattern_7 ();
  pattern_8 ();
  pattern_9 ();
  pattern_10 ();
  pattern_11 ();
  pattern_12 ();
  pattern_13 ();
  pattern_14 ();
  pattern_15 ();
  pattern_16 ();
  pattern_17 ();
  pattern_18 ();
  pattern_19 ();
  pattern_20 ();
  pattern_21 ();
  pattern_22 ();
  pattern_23 ();
  pattern_24 ();
  pattern_25 ();
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_1:\n"
     "   push %ebp\n"
     "   or   $0xffffffff, %ecx  # [ 0x83 0xc9 0xff ]\n"
     "   mov  %esp,%ebp\n"
     "   test %edx,%edx\n"
     "   call _func_under_pattern_1\n"
     "   pop %ebp\n"
     "   ret\n"
);

void func_under_pattern_1 (void)
{
   puts ("I am the function under pattern_1");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_2:\n"
     "   call L1           # [ 0xe0 0x00 0x00 000 0x00 ]\n"
     "L1:\n"
     "   pop   %ecx        # [ 0x59 ]\n"
     "   push  %ebp\n"
     "   mov   %esp, %ebp\n"
     "   sub   $0x18, %esp\n"
     "   call _func_under_pattern_2\n"
     "   add $0x18, %esp\n"
     "   pop %ebp\n"
     "   ret\n"
);

void func_under_pattern_2 (void)
{
   puts ("I am the function under pattern_2");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_3:\n"
     "   push %ebp\n"
     "   add %edx, %eax # [ 0x01 0xd0 ]\n"
     "   mov %esp, %ebp\n"
     "   push %ecx\n"
     "   push %ecx\n"
     "   call _func_under_pattern_3\n"
     "   pop %ecx\n"
     "   pop %ecx\n"
     "   pop %ebp\n"
     "   ret\n"
);

void func_under_pattern_3 (void)
{
   puts ("I am the function under pattern_3");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_4:\n"
     "   push %ebp\n"
     "   movzwl %ax,%eax # [ 0x0f 0xb7 0xc0 ]\n"
     "   mov %esp, %ebp\n"
     "   call _func_under_pattern_4\n"
     "   pop %ebp\n"
     "   ret\n"
);

void func_under_pattern_4 (void)
{
   puts ("I am the function under pattern_4");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_5:\n"
     "   push %ebp\n"
     "   movzwl %dx,%edx # [ 0x0f 0xb7 0xd2 ]\n"
     "   mov %esp, %ebp\n"
     "   call _func_under_pattern_5\n"
     "   pop %ebp\n"
     "   ret\n"
);

void func_under_pattern_5 (void)
{
   puts ("I am the function under pattern_5");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_6:\n"
     "   push %ebp\n"
     "   test %eax, %eax # [ 0x85 0xc0 ]\n"
     "   mov  %esp, %ebp\n"
     "   call _func_under_pattern_6\n"
     "   pop %ebp\n"
     "   ret\n"
);

void func_under_pattern_6 (void)
{
   puts ("I am the function under pattern_6");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_7:\n"
     "   push %ebp\n"
     "   test %edx,%edx  # [ 0x85 0xd2 ]\n"
     "   mov %esp,%ebp\n"
     "   call _func_under_pattern_7\n"
     "   pop %ebp\n"
     "   ret\n"
);

void func_under_pattern_7 (void)
{
   puts ("I am the function under pattern_7");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_8:\n"
     "  push %ebp\n"
     "  test $0xffffff80,%eax  # [ 0xa9 0x80 0xff 0xff 0xff ]\n"
     "  mov %esp,%ebp\n"
     "   call _func_under_pattern_8\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_8 (void)
{
   puts ("I am the function under pattern_8");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_9:\n"
     "  push %ebp\n"
     "  inc %eax # [ 0x40 ]\n"
     "  mov %esp,%ebp\n"
     "   call _func_under_pattern_9\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_9 (void)
{
   puts ("I am the function under pattern_9");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_10:\n"
     "  push %ebp\n"
     "  add  $0x3, %ecx # [ 0x83 0xc1 0x03 ]\n"
     "  mov %esp,%ebp\n"
     "   call _func_under_pattern_10\n"
     "  pop  %ebp\n"
     "  ret\n"
);

void func_under_pattern_10 (void)
{
   puts ("I am the function under pattern_10");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_11:\n"
     "  push %ebp\n"
     "  mov %eax,%ecx # [ 0x89 0xc1 ]\n"
     "  mov %esp,%ebp\n"
     "   call _func_under_pattern_11\n"
     "  pop  %ebp\n"
     "  ret\n"
);

void func_under_pattern_11 (void)
{
   puts ("I am the function under pattern_11");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_12:\n"
     "  push %ebp\n"
     "  cld # [ 0xfc ]\n"
     "  xor  %eax,%eax\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_12\n"
     "  pop  %ebp\n"
     "  ret\n"
);

void func_under_pattern_12 (void)
{
   puts ("I am the function under pattern_12");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_13:\n"
     "  push %ebp\n"
     "  dec %edx # [ 0x4a ]\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_13\n"
     "  pop  %ebp\n"
     "  ret\n"
);

void func_under_pattern_13 (void)
{
   puts ("I am the function under pattern_13");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_14:\n"
     "  mov  %eax, %ecx # [ 0x89 0xc1 ]\n"
     "  mov  %edx, %eax # [ 0x89 0xd0 ]\n"
     "  xor  %edx, %edx # [ 0x31 0xd2 ]\n"
     "  div  %ecx # [ 0xf7 0xf1 ]\n"
     "  push %ebp\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_14\n"
     "  pop  %ebp\n"
     "  ret\n"
);

void func_under_pattern_14 (void)
{
   puts ("I am the function under pattern_14");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_15:\n"
     "  push  %ebp\n"
     "  fldz # [ 0xd9 0xee ]\n"
     "  mov   %esp, %ebp\n"
     "   call _func_under_pattern_15\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_15 (void)
{
   puts ("I am the function under pattern_15");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_16:\n"
     "  and  $0x3d, %eax # [ 0x83 0xe0 0x3d ]\n"
     "  push %ebp\n"
     "  sub  $0x8, %eax # [ 0x83 0xe8 0x08 ]\n"
     "  mov  %esp, %ebp\n"
     "   call _func_under_pattern_16\n"
     "  pop  %ebp\n"
     "  ret\n"
);

void func_under_pattern_16 (void)
{
   puts ("I am the function under pattern_16");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_17:\n"
     "  push  %ebp\n"
     "  cmpl  $0x0,_main # From mach_kernel: [ 0x83 0x3d 0x54 0x42 0x3a 0xc0 0x00 ]\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_17\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_17 (void)
{
   puts ("I am the function under pattern_17");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_18:\n"
     "  push %ebp\n"
     "  mov $0xffffffce, %eax # [ 0xb8 0xce 0xff 0xff 0xff ]\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_18\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_18 (void)
{
   puts ("I am the function under pattern_18");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_19:\n"
     "  cmp $0x0, %edx # [ 0x83 0xfa 0x00 ]\n"
     "  je LM2  # [ 0x74 0x54 (54 being a disp8, so 0x00 in this case) ]\n"
     "LM2:\n"
     "  push %ebp\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_19\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_19 (void)
{
   puts ("I am the function under pattern_19");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_20:\n"
     "  push %eax\n"
     "  pop  %eax # [ 0x58 ]\n"
     "  mov  $_main, %eax\n"
     "  pushl 0x104(%eax) # [ 0xff 0xb0 0x7f 0xf8 0xff 0x0f ]\n"
     "  pop  %eax\n"
     "  push %ebp\n"
     "  jmp LM3   # Skip over some instructions that aren't actually correct here..\n"
     "  mov 268436759(%eax), %eax # [ 0x8b 0x80 0x27 0x05 0x00 0x10 ]\n"
     "  jmp *%eax # [ 0xff 0xe0 ]\n"
     "LM3:\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_20\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_20 (void)
{
   puts ("I am the function under pattern_20");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_21:\n"
     "  push %ebp\n"
     "  mov  $0x18, %dx # [ 0x66 0xba 0x18 0x00 ]\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_21\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_21 (void)
{
   puts ("I am the function under pattern_21");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_22:\n"
     "  nop # [ 0x90 ]\n"
     "  nop\n"
     "  nop\n"
     "  nop\n"
     "  nop\n"
     "  nop\n"
     "  push %ebp\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_22\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_22 (void)
{
   puts ("I am the function under pattern_22");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_23:\n"
     "  push %ebp\n"
     "  movzbl %al, %eax # [ 0x0f 0xb6 0xc0 ]\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_23\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_23 (void)
{
   puts ("I am the function under pattern_23");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_24:\n"
     "  push %ebp\n"
     "  mov %eax, %edx # [ 0x89 0xc2 ]\n"
     "  sar $0x8, %eax # [ 0xc1 0xf8 0x08 ]\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_24\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_24 (void)
{
   puts ("I am the function under pattern_24");
}

asm ("  .text\n"
     "  .align 8\n"
     "_pattern_25:\n"
     "  push %ebp\n"
     "  and  $0xffffff, %eax # [ 0x25 0xff 0xff 0xff 0x00 ]\n"
     "  mov %esp, %ebp\n"
     "   call _func_under_pattern_25\n"
     "  pop %ebp\n"
     "  ret\n"
);

void func_under_pattern_25 (void)
{
   puts ("I am the function under pattern_25");
}

