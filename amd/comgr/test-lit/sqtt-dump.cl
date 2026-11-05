// RUN: %clang --target=amdgcn-amd-amdhsa -mcpu=gfx900 -nogpulib -O1 \
// RUN:   -fsqtt-instrument %s -o %t.so
// RUN: sqtt-dump %t.so | %FileCheck %s --check-prefixes=CHECK,NODEBUG

// RUN: %clang --target=amdgcn-amd-amdhsa -mcpu=gfx900 -nogpulib -O1 \
// RUN:   -gline-tables-only -fsqtt-instrument %s -o %t.g.so
// RUN: sqtt-dump %t.g.so | %FileCheck %s --check-prefixes=CHECK,DEBUG

// RUN: %clang --target=amdgcn-amd-amdhsa -mcpu=gfx900 -nogpulib -O1 \
// RUN:   -fsqtt-instrument -mllvm -amdgpu-enable-object-linking %s \
// RUN:   -o %t.link.so
// RUN: sqtt-dump %t.link.so | %FileCheck %s --check-prefix=LINK

// CHECK: record 0: pc={{[0-9]+}} events=3
// CHECK-NEXT:   UserExit region
// CHECK-NEXT:   FunctionExit __clang_ocl_kern_imp_foo
// CHECK-NEXT:   FunctionExit foo
// NODEBUG-NEXT: location: foo at ??:0:0
// DEBUG-NEXT: location: foo at {{.*\.cl:[0-9]+:[0-9]+}}
// DEBUG-NEXT: (inlined by) foo at {{.*\.cl:[0-9]+:[0-9]+}}
// CHECK: record 1: pc={{[0-9]+}} events=2
// CHECK-NEXT:   FunctionEntry __clang_ocl_kern_imp_foo
// CHECK-NEXT:   UserEntry region
// NODEBUG-NEXT: location: __clang_ocl_kern_imp_foo at ??:0:0
// DEBUG-NEXT: location: __clang_ocl_kern_imp_foo at {{.*\.cl:[0-9]+:[0-9]+}}
// CHECK: record 2: pc={{[0-9]+}} events=3
// CHECK-NEXT:   FunctionEntry foo
// CHECK-NEXT:   FunctionEntry __clang_ocl_kern_imp_foo
// CHECK-NEXT:   UserEntry region
// NODEBUG-NEXT: location: foo at ??:0:0
// DEBUG-NEXT: location: foo at {{.*\.cl:[0-9]+:[0-9]+}}
// CHECK: record 3: pc={{[0-9]+}} events=1
// CHECK-NEXT:   FunctionExit helper
// NODEBUG-NEXT: location: helper at ??:0:0
// DEBUG-NEXT: location: helper at {{.*\.cl:[0-9]+:[0-9]+}}
// CHECK: record 4: pc={{[0-9]+}} events=1
// CHECK-NEXT:   FunctionEntry helper
// NODEBUG-NEXT: location: helper at ??:0:0
// DEBUG-NEXT: location: helper at {{.*\.cl:[0-9]+:[0-9]+}}
// CHECK: record 5: pc={{[0-9]+}} events=2
// CHECK-NEXT:   UserExit region
// CHECK-NEXT:   FunctionExit __clang_ocl_kern_imp_foo
// NODEBUG-NEXT: location: __clang_ocl_kern_imp_foo at ??:0:0
// DEBUG-NEXT: location: __clang_ocl_kern_imp_foo at {{.*\.cl:[0-9]+:[0-9]+}}
// CHECK-NOT: record {{[0-9]+}}:

// LINK: record 0: pc={{[0-9]+}}
// LINK-NEXT:   UserExit region
// LINK-NEXT:   FunctionExit __clang_ocl_kern_imp_foo
// LINK-NEXT:   location: __clang_ocl_kern_imp_foo at ??:0:0
// LINK: record 1: pc={{[0-9]+}}
// LINK-NEXT:   SetEventTableBase 0
// LINK-NEXT:   UserExit region
// LINK-NEXT:   FunctionExit __clang_ocl_kern_imp_foo
// LINK-NEXT:   FunctionExit foo
// LINK-NEXT:   location: foo at ??:0:0
// LINK: record 2: pc={{[0-9]+}}
// LINK-NEXT:   FunctionEntry __clang_ocl_kern_imp_foo
// LINK-NEXT:   UserEntry region
// LINK-NEXT:   location: __clang_ocl_kern_imp_foo at ??:0:0
// LINK: record 3: pc={{[0-9]+}}
// LINK-NEXT:   SetEventTableBase 0
// LINK-NEXT:   location: foo at ??:0:0
// LINK: record 4: pc={{[0-9]+}}
// LINK-NEXT:   SetEventTableBase 0
// LINK-NEXT:   FunctionEntry helper
// LINK-NEXT:   location: helper at ??:0:0
// LINK: record 5: pc={{[0-9]+}}
// LINK-NEXT:   FunctionEntry foo
// LINK-NEXT:   FunctionEntry __clang_ocl_kern_imp_foo
// LINK-NEXT:   UserEntry region
// LINK-NEXT:   SetEventTableBase 0
// LINK-NEXT:   location: foo at ??:0:0
// LINK: record 6: pc={{[0-9]+}}
// LINK-NEXT:   FunctionExit helper
// LINK-NEXT:   SetEventTableBase 0
// LINK-NEXT:   location: helper at ??:0:0
// LINK-NOT: record {{[0-9]+}}:

__attribute__((noinline)) void helper(global int *Out) { *Out = 1; }

kernel void foo(global int *Out) {
  __builtin_amdgcn_sqtt_user_entry("region");
  helper(Out);
  __builtin_amdgcn_sqtt_user_exit("region");
}
