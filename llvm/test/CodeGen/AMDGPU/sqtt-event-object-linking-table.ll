; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking -filetype=obj < %S/sqtt-event-object-linking.ll | llvm-objdump --sqtt - | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-enable-object-linking -filetype=obj < %S/sqtt-event-object-linking.ll | llvm-objdump --sqtt - | FileCheck %s

; CHECK:      [
; CHECK-NEXT:   [
; CHECK-NEXT:     {
; CHECK-NEXT:       "type": "SetEventTableBase",
; CHECK-NEXT:       "payload": 0,
; CHECK-NEXT:       "file": "sqtt-event-object-linking.ll",
; CHECK-NEXT:       "line": 46,
; CHECK-NEXT:       "column": 3
; CHECK-NEXT:     }
; CHECK-NEXT:   ],
; CHECK-NEXT:   [
; CHECK-NEXT:     {
; CHECK-NEXT:       "type": "SetEventTableBase",
; CHECK-NEXT:       "payload": 0,
; CHECK-NEXT:       "file": "sqtt-event-object-linking.ll",
; CHECK-NEXT:       "line": 43,
; CHECK-NEXT:       "column": 3
; CHECK-NEXT:     }
; CHECK-NEXT:   ],
; CHECK-NEXT:   [
; CHECK-NEXT:     {
; CHECK-NEXT:       "type": "FunctionEntry",
; CHECK-NEXT:       "payload": "sqtt_event",
; CHECK-NEXT:       "file": "sqtt-event-object-linking.ll",
; CHECK-NEXT:       "line": 44,
; CHECK-NEXT:       "column": 3
; CHECK-NEXT:     }
; CHECK-NEXT:   ]
; CHECK-NEXT: ]
