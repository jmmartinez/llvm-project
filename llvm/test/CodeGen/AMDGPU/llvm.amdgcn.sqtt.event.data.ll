; RUN: not --crash llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -filetype=null < %s 2>&1 | FileCheck --check-prefix=SDAG %s
; RUN: not llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -filetype=null < %s 2>&1 | FileCheck --check-prefix=GISEL %s

; Extra data operands on llvm.amdgcn.sqtt.event are not selected yet.

; SDAG: LLVM ERROR: Cannot select: intrinsic %llvm.amdgcn.sqtt.event
; GISEL: LLVM ERROR: cannot select: G_INTRINSIC_W_SIDE_EFFECTS intrinsic(@llvm.amdgcn.sqtt.event),

define void @event_with_data(i32 %data) {
  call void (metadata, i32, ...) @llvm.amdgcn.sqtt.event(metadata !0, i32 poison, i32 %data)
  ret void
}

declare void @llvm.amdgcn.sqtt.event(metadata, i32, ...)

!0 = !{i32 0, !"event"}
