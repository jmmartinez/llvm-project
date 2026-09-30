; RUN: not --crash llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking -filetype=null %s 2>&1 | FileCheck --check-prefix=SDAG %s
; RUN: not llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking -filetype=null %s 2>&1 | FileCheck --check-prefix=GISEL %s

; SDAG: LLVM ERROR: Cannot select: intrinsic %llvm.amdgcn.sqtt.event
; GISEL: LLVM ERROR: cannot select: G_INTRINSIC_W_SIDE_EFFECTS intrinsic(@llvm.amdgcn.sqtt.event)

declare void @llvm.amdgcn.sqtt.event(metadata, i32, i1 immarg)

define void @sqtt_event() {
  call void @llvm.amdgcn.sqtt.event(metadata !0, i32 poison, i1 true)
  ret void
}

!0 = !{i32 0, !"sqtt_event"}
