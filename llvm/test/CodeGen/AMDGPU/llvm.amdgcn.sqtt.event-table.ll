; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %s | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %s | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %s | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %s | FileCheck %s

define void @sqtt_event_entry() {
  call void @llvm.amdgcn.sqtt.event(metadata !0, i32 poison)
  ret void
}

define void @sqtt_event_exit() {
  call void @llvm.amdgcn.sqtt.event(metadata !1, i32 poison)
  ret void
}

define void @sqtt_events_merged() {
  call void @llvm.amdgcn.sqtt.event(metadata !2, i32 poison)
  call void @llvm.amdgcn.sqtt.event(metadata !3, i32 poison)
  ret void
}

declare void @llvm.amdgcn.sqtt.event(metadata, i32)

!0 = !{i32 0, !"sqtt_event_entry"}
!1 = !{i32 1, !"sqtt_event_exit"}
!2 = !{i32 0, !"sqtt_events_merged"}
!3 = !{i32 1, !"sqtt_events_merged"}

; CHECK:      .section{{.*}}__sqtt_events,"",@progbits
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.2

; CHECK:      .section{{.*}}__sqtt_data,"",@progbits
; CHECK-NEXT: .Lsqtt_data.0:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.0
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.3
; CHECK-NEXT: .Lsqtt_data.1:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.1
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.4
; CHECK-NEXT: .Lsqtt_data.2:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.2
; CHECK-NEXT: .short{{.*}}2
; CHECK-NEXT: .short{{.*}}0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.5
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.5
; CHECK-NEXT: .Lsqtt_data.3:
; CHECK-NEXT: .ascii{{.*}}"sqtt_event_entry"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_data.4:
; CHECK-NEXT: .ascii{{.*}}"sqtt_event_exit"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_data.5:
; CHECK-NEXT: .ascii{{.*}}"sqtt_events_merged"
; CHECK-NEXT: .byte{{.*}}0
