// RUN: %clang --target=amdgcn-amd-amdhsa -mcpu=gfx900 -nogpulib -O1 \
// RUN:   -fsqtt-instrument %s -o %t.so
// RUN: sqtt-dump %t.so | %FileCheck %s

// CHECK: records: 6

__attribute__((noinline)) void helper(global int *Out) { *Out = 1; }

kernel void foo(global int *Out) {
  __builtin_amdgcn_sqtt_user_entry("region");
  helper(Out);
  __builtin_amdgcn_sqtt_user_exit("region");
}
