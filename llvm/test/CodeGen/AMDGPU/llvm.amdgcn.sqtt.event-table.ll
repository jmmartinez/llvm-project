; RUN: llc -global-isel=0 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s
; RUN: llc -global-isel=0 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s
; RUN: llc -global-isel=1 -mtriple=amdgpu10.10-amd-amdhsa -mcpu=gfx1010 -filetype=obj < %S/llvm.amdgcn.sqtt.event.ll | llvm-objdump --sqtt - | FileCheck %s

; CHECK:      [
; CHECK-NEXT:   {
; CHECK-NEXT:     "pc": {{[0-9]+}},
; CHECK-NEXT:     "file": "llvm.amdgcn.sqtt.event.ll",
; CHECK-NEXT:     "line": 27,
; CHECK-NEXT:     "column": 3,
; CHECK-NEXT:     "events": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "type": "FunctionEntry",
; CHECK-NEXT:         "payload": "sqtt_event_entry"
; CHECK-NEXT:       }
; CHECK-NEXT:     ]
; CHECK-NEXT:   },
; CHECK-NEXT:   {
; CHECK-NEXT:     "pc": {{[0-9]+}},
; CHECK-NEXT:     "file": "llvm.amdgcn.sqtt.event.ll",
; CHECK-NEXT:     "line": 51,
; CHECK-NEXT:     "column": 3,
; CHECK-NEXT:     "events": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "type": "FunctionExit",
; CHECK-NEXT:         "payload": "sqtt_event_exit"
; CHECK-NEXT:       }
; CHECK-NEXT:     ]
; CHECK-NEXT:   },
; CHECK-NEXT:   {
; CHECK-NEXT:     "pc": {{[0-9]+}},
; CHECK-NEXT:     "file": "llvm.amdgcn.sqtt.event.ll",
; CHECK-NEXT:     "line": 75,
; CHECK-NEXT:     "column": 3,
; CHECK-NEXT:     "events": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "type": "FunctionEntry",
; CHECK-NEXT:         "payload": "sqtt_events_merged"
; CHECK-NEXT:       },
; CHECK-NEXT:       {
; CHECK-NEXT:         "type": "FunctionExit",
; CHECK-NEXT:         "payload": "sqtt_events_merged"
; CHECK-NEXT:       }
; CHECK-NEXT:     ]
; CHECK-NEXT:   },
; CHECK-NEXT:   {
; CHECK-NEXT:     "pc": {{[0-9]+}},
; CHECK-NEXT:     "file": "llvm.amdgcn.sqtt.event.ll",
; CHECK-NEXT:     "line": 100,
; CHECK-NEXT:     "column": 3,
; CHECK-NEXT:     "events": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "type": "UserEntry",
; CHECK-NEXT:         "payload": "user_entry"
; CHECK-NEXT:       }
; CHECK-NEXT:     ]
; CHECK-NEXT:   },
; CHECK-NEXT:   {
; CHECK-NEXT:     "pc": {{[0-9]+}},
; CHECK-NEXT:     "file": "llvm.amdgcn.sqtt.event.ll",
; CHECK-NEXT:     "line": 124,
; CHECK-NEXT:     "column": 3,
; CHECK-NEXT:     "events": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "type": "UserExit",
; CHECK-NEXT:         "payload": "user_exit"
; CHECK-NEXT:       }
; CHECK-NEXT:     ]
; CHECK-NEXT:   }
; CHECK-NEXT: ]
