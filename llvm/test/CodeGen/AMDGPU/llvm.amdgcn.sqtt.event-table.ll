; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %s | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %s | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %s | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %s | FileCheck %s

define void @sqtt_events() {
  call void @llvm.amdgcn.sqtt.event(metadata !0, i32 poison)
  call void @llvm.amdgcn.sqtt.event(metadata !1, i32 poison)
  ret void
}

declare void @llvm.amdgcn.sqtt.event(metadata, i32)

!0 = !{i32 0, !"sqtt_events"}
!1 = !{i32 1, !"sqtt_events"}

; CHECK:      .section{{.*}}__sqtt_events,"",@progbits
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.0
; CHECK-NEXT: .short{{.*}}0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event_str.0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.1
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event_str.0

; CHECK:      .section{{.*}}__sqtt_strings,"",@progbits
; CHECK-NEXT: .Lsqtt_event_str.0:
; CHECK-NEXT: .ascii{{.*}}"sqtt_events"
; CHECK-NEXT: .byte{{.*}}0
