// RUN: %clang --target=amdgcn-amd-amdhsa -mcpu=gfx900 -nogpulib -O1 \
// RUN:   -fsqtt-instrument %s -o %t.so
// RUN: not sqtt-dump %t.so 2>&1 | %FileCheck %s

// CHECK: amd_comgr_create_sqtt_table: ERROR

__attribute__((noinline)) void helper(global int *Out) { *Out = 1; }

kernel void foo(global int *Out) {
  __builtin_amdgcn_sqtt_user_entry("region");
  helper(Out);
  __builtin_amdgcn_sqtt_user_exit("region");
}
