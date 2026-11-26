// REQUIRES: amdgpu-registered-target
// RUN: %clang_cc1 -triple amdgpu-amd-amdhsa -fcuda-is-device -fsqtt-instrument -mllvm -amdgpu-sqtt-function-instrument-small-function-limit=0 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple amdgpu-amd-amdhsa -fcuda-is-device -emit-llvm -o - %s | FileCheck --check-prefix=NONE %s
// RUN: %clang_cc1 -triple amdgpu-amd-amdhsa -fcuda-is-device -fsqtt-instrument=none -emit-llvm -o - %s | FileCheck --check-prefix=NONE %s

#define __device__ __attribute__((device))

// NONE-NOT: @llvm.amdgcn.sqtt.event
// CHECK-LABEL: @test_user_markers(
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[FUNC_ENTRY:![0-9]+]], i32 poison, i32 0)
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[ENTRY:![0-9]+]], i32 poison, i32 0)
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[EXIT:![0-9]+]], i32 poison, i32 0)
// CHECK: call void @llvm.amdgcn.sqtt.event(metadata [[FUNC_EXIT:![0-9]+]], i32 poison, i32 0)
extern "C" __device__ void test_user_markers() {
  __builtin_amdgcn_sqtt_user_entry("foo");
  __builtin_amdgcn_sqtt_user_exit("foo");
}

// CHECK: [[FUNC_ENTRY]] = !{i32 0, !"test_user_markers"}
// CHECK: [[ENTRY]] = !{i32 2, !"foo"}
// CHECK: [[EXIT]] = !{i32 3, !"foo"}
// CHECK: [[FUNC_EXIT]] = !{i32 1, !"test_user_markers"}
