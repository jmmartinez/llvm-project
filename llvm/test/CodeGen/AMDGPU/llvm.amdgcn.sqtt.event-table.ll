; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefix=OBJDUMP
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefix=OBJDUMP
; RUN: llc -global-isel=0 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefix=OBJDUMP
; RUN: llc -global-isel=1 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefix=OBJDUMP

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
; CHECK-NEXT: .quad{{.*}}.Lsqtt_str.0
; CHECK-NEXT: .Lsqtt_data.1:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.1
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_str.1
; CHECK-NEXT: .Lsqtt_data.2:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.2
; CHECK-NEXT: .short{{.*}}2
; CHECK-NEXT: .short{{.*}}0
; CHECK-NEXT: .quad{{.*}}.Lsqtt_str.2
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .quad{{.*}}.Lsqtt_str.2
; CHECK-NEXT: .Lsqtt_data.3:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.3
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}2
; CHECK-NEXT: .quad{{.*}}.Lsqtt_str.3
; CHECK-NEXT: .Lsqtt_data.4:
; CHECK-NEXT: .quad{{.*}}.Lsqtt_event.4
; CHECK-NEXT: .short{{.*}}1
; CHECK-NEXT: .short{{.*}}3
; CHECK-NEXT: .quad{{.*}}.Lsqtt_str.4
; CHECK:      .section{{.*}}__sqtt_data_str,"MS",@progbits
; CHECK-NEXT: .Lsqtt_str.0:
; CHECK-NEXT: .ascii{{.*}}"sqtt_event_entry"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_str.1:
; CHECK-NEXT: .ascii{{.*}}"sqtt_event_exit"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_str.2:
; CHECK-NEXT: .ascii{{.*}}"sqtt_events_merged"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_str.3:
; CHECK-NEXT: .ascii{{.*}}"user_entry"
; CHECK-NEXT: .byte{{.*}}0
; CHECK-NEXT: .Lsqtt_str.4:
; CHECK-NEXT: .ascii{{.*}}"user_exit"
; CHECK-NEXT: .byte{{.*}}0

; FIXME: The pointers in __sqtt_events and __sqtt_data are R_AMDGPU_ABS64
; relocations, which llvm-objdump --sqtt does not apply yet. Every entry reads
; as a pointer to the first record, with a zero address and payload.
; OBJDUMP:      [
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionEntry",
; OBJDUMP-NEXT:       "payload": 0,
; OBJDUMP-NEXT:       "address": 0
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionEntry",
; OBJDUMP-NEXT:       "payload": 0,
; OBJDUMP-NEXT:       "address": 0
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionEntry",
; OBJDUMP-NEXT:       "payload": 0,
; OBJDUMP-NEXT:       "address": 0
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionEntry",
; OBJDUMP-NEXT:       "payload": 0,
; OBJDUMP-NEXT:       "address": 0
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionEntry",
; OBJDUMP-NEXT:       "payload": 0,
; OBJDUMP-NEXT:       "address": 0
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ]
; OBJDUMP-NEXT: ]
