; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s

; CHECK:      .section{{.*}}__sqtt_events,"",@progbits
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.2
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.3
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.4

; CHECK:      .section{{.*}}__sqtt_data,"",@progbits
; CHECK-NEXT: .Lsqtt_data.0:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.0
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.5
; CHECK-NEXT: .Lsqtt_data.1:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.1
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.6
; CHECK-NEXT: .Lsqtt_data.2:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.2
; CHECK-NEXT: .short{{.*}}2
; CHECK-NEXT: .short{{.*}}0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.7
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.7
; CHECK-NEXT: .Lsqtt_data.3:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.3
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}2
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.8
; CHECK-NEXT: .Lsqtt_data.4:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.4
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}3
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.9
; CHECK-NEXT: .Lsqtt_data.5:
; CHECK-NEXT: .ascii{{.*}}"sqtt_event_entry"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_data.6:
; CHECK-NEXT: .ascii{{.*}}"sqtt_event_exit"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_data.7:
; CHECK-NEXT: .ascii{{.*}}"sqtt_events_merged"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_data.8:
; CHECK-NEXT: .ascii{{.*}}"user_entry"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_data.9:
; CHECK-NEXT: .ascii{{.*}}"user_exit"
; CHECK-NEXT: .byte{{.*}}0
