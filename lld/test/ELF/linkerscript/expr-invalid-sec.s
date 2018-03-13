# REQUIRES: x86
# RUN: llvm-mc -filetype=obj -triple=x86_64-unknown-linux /dev/null -o %t.o
# RUN: not ld.lld -o %t.so --script %s %t.o -shared 2>&1 | FileCheck %s

# CHECK: error: {{.*}}.s:8: at least one side of the expression must be absolute

SECTIONS {
  foo = ADDR(.text) + ADDR(.text);
};
