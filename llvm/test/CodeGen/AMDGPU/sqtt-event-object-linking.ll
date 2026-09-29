; RUN: not --crash llc -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking -filetype=null %s 2>&1 | FileCheck %s

; CHECK: error: SQTT event lowering assumes a single translation unit and is incompatible with -amdgpu-enable-object-linking
; CHECK: LLVM ERROR: Cannot select: intrinsic %llvm.amdgcn.sqtt.event

declare void @llvm.amdgcn.sqtt.event(metadata, i32)

define void @sqtt_event() {
  call void @llvm.amdgcn.sqtt.event(metadata !0, i32 poison)
  ret void
}

!0 = !{i32 0, !"sqtt_event"}
