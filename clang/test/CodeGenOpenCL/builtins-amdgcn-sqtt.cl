// REQUIRES: amdgpu-registered-target
// RUN: %clang_cc1 -cl-std=CL2.0 -triple amdgcn-unknown-unknown -emit-llvm -o - %s | FileCheck %s

// CHECK-LABEL: @test_user_markers(
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[ENTRY:![0-9]+]], i32 poison)
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[EXIT:![0-9]+]], i32 poison)
void test_user_markers() {
  __builtin_amdgcn_sqtt_user_entry("foo");
  __builtin_amdgcn_sqtt_user_exit("foo");
}

// CHECK: [[ENTRY]] = !{i32 2, !"foo"}
// CHECK: [[EXIT]] = !{i32 3, !"foo"}
