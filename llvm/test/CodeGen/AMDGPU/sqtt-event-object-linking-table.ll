; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking < %S/sqtt-event-object-linking.ll | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking < %S/sqtt-event-object-linking.ll | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking -filetype=obj < %S/sqtt-event-object-linking.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefix=OBJDUMP
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking -filetype=obj < %S/sqtt-event-object-linking.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefix=OBJDUMP

; CHECK:      .section{{.*}}__sqtt_events,"",@progbits
; CHECK-NEXT: .Lsqtt_events_cu_base:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_data.2

; CHECK:      .section{{.*}}__sqtt_data,"",@progbits
; CHECK-NEXT: .Lsqtt_data.0:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.0
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}4
; CHECK-NEXT: .quad{{.*}}__start___sqtt_events
; CHECK-NEXT: .Lsqtt_data.1:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.1
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}4
; CHECK-NEXT: .quad{{.*}}.Lsqtt_events_cu_base+0
; CHECK-NEXT: .Lsqtt_data.2:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.2
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_str.0
; CHECK:      .section{{.*}}__sqtt_data_str,"MS",@progbits
; CHECK-NEXT: .Lsqtt_str.0:
; CHECK-NEXT: .ascii{{.*}}"sqtt_event"
; CHECK-NEXT: .byte{{.*}}0

; OBJDUMP:      [
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "SetEventTableBase",
; OBJDUMP-NEXT:       "payload": 0,
; OBJDUMP-NEXT:       "address": 108
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "SetEventTableBase",
; OBJDUMP-NEXT:       "payload": 0,
; OBJDUMP-NEXT:       "address": 60
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionEntry",
; OBJDUMP-NEXT:       "payload": "sqtt_event",
; OBJDUMP-NEXT:       "address": 84
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ]
; OBJDUMP-NEXT: ]
