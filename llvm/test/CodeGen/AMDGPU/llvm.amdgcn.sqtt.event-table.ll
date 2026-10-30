; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 < %S/llvm.amdgcn.sqtt.event.ll | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefixes=OBJDUMP,OBJDUMP-GFX9
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefixes=OBJDUMP,OBJDUMP-GFX9
; RUN: llc -global-isel=0 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefixes=OBJDUMP,OBJDUMP-GFX10
; RUN: llc -global-isel=1 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s --check-prefixes=OBJDUMP,OBJDUMP-GFX10

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

; OBJDUMP:      [
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionEntry",
; OBJDUMP-NEXT:       "payload": "sqtt_event_entry",
; OBJDUMP-GFX9-NEXT:  "address": 12
; OBJDUMP-GFX10-NEXT: "address": 4
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionExit",
; OBJDUMP-NEXT:       "payload": "sqtt_event_exit",
; OBJDUMP-GFX9-NEXT:  "address": 76
; OBJDUMP-GFX10-NEXT: "address": 68
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionEntry",
; OBJDUMP-NEXT:       "payload": "sqtt_events_merged",
; OBJDUMP-GFX9-NEXT:  "address": 140
; OBJDUMP-GFX10-NEXT: "address": 132
; OBJDUMP-NEXT:     },
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "FunctionExit",
; OBJDUMP-NEXT:       "payload": "sqtt_events_merged",
; OBJDUMP-GFX9-NEXT:  "address": 140
; OBJDUMP-GFX10-NEXT: "address": 132
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "UserEntry",
; OBJDUMP-NEXT:       "payload": "user_entry",
; OBJDUMP-GFX9-NEXT:  "address": 204
; OBJDUMP-GFX10-NEXT: "address": 196
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ],
; OBJDUMP-NEXT:   [
; OBJDUMP-NEXT:     {
; OBJDUMP-NEXT:       "type": "UserExit",
; OBJDUMP-NEXT:       "payload": "user_exit",
; OBJDUMP-GFX9-NEXT:  "address": 268
; OBJDUMP-GFX10-NEXT: "address": 260
; OBJDUMP-NEXT:     }
; OBJDUMP-NEXT:   ]
; OBJDUMP-NEXT: ]
