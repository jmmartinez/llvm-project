// REQUIRES: amdgpu-registered-target
// RUN: %clang_cc1 -cl-std=CL2.0 -triple amdgcn-unknown-unknown -fsqtt-instrument -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -cl-std=CL2.0 -triple amdgcn-unknown-unknown -emit-llvm -o - %s | FileCheck --check-prefix=NONE %s
// RUN: %clang_cc1 -cl-std=CL2.0 -triple amdgcn-unknown-unknown -fsqtt-instrument=none -emit-llvm -o - %s | FileCheck --check-prefix=NONE %s

// NONE-NOT: @llvm.amdgcn.sqtt.event
// CHECK-LABEL: @test_user_markers(
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[FUNC_ENTRY:![0-9]+]], i32 poison, i1 true)
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[ENTRY:![0-9]+]], i32 poison, i1 true)
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[EXIT:![0-9]+]], i32 poison, i1 true)
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[FUNC_EXIT:![0-9]+]], i32 poison, i1 true)
void test_user_markers() {
  __builtin_amdgcn_sqtt_user_entry("foo");
  __builtin_amdgcn_sqtt_user_exit("foo");
}

// CHECK: [[FUNC_ENTRY]] = !{i32 0, !"test_user_markers"}
// CHECK: [[ENTRY]] = !{i32 2, !"foo"}
// CHECK: [[EXIT]] = !{i32 3, !"foo"}
// CHECK: [[FUNC_EXIT]] = !{i32 1, !"test_user_markers"}
