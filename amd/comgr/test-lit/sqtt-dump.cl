// RUN: %clang --target=amdgcn-amd-amdhsa -mcpu=gfx900 -nogpulib -O1 \
// RUN:   -fsqtt-instrument %s -o %t.so
// RUN: sqtt-dump %t.so | %FileCheck %s

// CHECK: record 0: pc={{[0-9]+}} events=3
// CHECK-NEXT:   UserExit region
// CHECK-NEXT:   FunctionExit __clang_ocl_kern_imp_foo
// CHECK-NEXT:   FunctionExit foo
// CHECK-NEXT: record 1: pc={{[0-9]+}} events=2
// CHECK-NEXT:   FunctionEntry __clang_ocl_kern_imp_foo
// CHECK-NEXT:   UserEntry region
// CHECK-NEXT: record 2: pc={{[0-9]+}} events=3
// CHECK-NEXT:   FunctionEntry foo
// CHECK-NEXT:   FunctionEntry __clang_ocl_kern_imp_foo
// CHECK-NEXT:   UserEntry region
// CHECK-NEXT: record 3: pc={{[0-9]+}} events=1
// CHECK-NEXT:   FunctionExit helper
// CHECK-NEXT: record 4: pc={{[0-9]+}} events=1
// CHECK-NEXT:   FunctionEntry helper
// CHECK-NEXT: record 5: pc={{[0-9]+}} events=2
// CHECK-NEXT:   UserExit region
// CHECK-NEXT:   FunctionExit __clang_ocl_kern_imp_foo
// CHECK-NOT: record {{[0-9]+}}:

__attribute__((noinline)) void helper(global int *Out) { *Out = 1; }

kernel void foo(global int *Out) {
  __builtin_amdgcn_sqtt_user_entry("region");
  helper(Out);
  __builtin_amdgcn_sqtt_user_exit("region");
}
