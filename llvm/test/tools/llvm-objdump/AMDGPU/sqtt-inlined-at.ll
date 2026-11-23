; REQUIRES: amdgpu-registered-target

; RUN: llc -mtriple=amdgpu9.00-amd-amdhsa -mcpu=gfx900 -amdgpu-sqtt-combine-events-threshold=0 -filetype=obj < %s | llvm-objdump --sqtt - | FileCheck %s

; CHECK:      [
; CHECK-NEXT:   {
; CHECK-NEXT:     "pc": {{[0-9]+}},
; CHECK-NEXT:     "locations": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "file": "sqtt-inlined-at.ll",
; CHECK-NEXT:         "line": 10,
; CHECK-NEXT:         "column": 3
; CHECK-NEXT:       },
; CHECK-NEXT:       {
; CHECK-NEXT:         "file": "sqtt-inlined-at.ll",
; CHECK-NEXT:         "line": 20,
; CHECK-NEXT:         "column": 5
; CHECK-NEXT:       },
; CHECK-NEXT:       {
; CHECK-NEXT:         "file": "sqtt-inlined-at.ll",
; CHECK-NEXT:         "line": 30,
; CHECK-NEXT:         "column": 7
; CHECK-NEXT:       }
; CHECK-NEXT:     ],
; CHECK-NEXT:     "events": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "type": "UserEntry",
; CHECK-NEXT:         "payload": "inlined"
; CHECK-NEXT:       }
; CHECK-NEXT:     ]
; CHECK-NEXT:   },
; CHECK-NEXT:   {
; CHECK-NEXT:     "pc": {{[0-9]+}},
; CHECK-NEXT:     "locations": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "file": "sqtt-inlined-at.ll",
; CHECK-NEXT:         "line": 31,
; CHECK-NEXT:         "column": 3
; CHECK-NEXT:       }
; CHECK-NEXT:     ],
; CHECK-NEXT:     "events": [
; CHECK-NEXT:       {
; CHECK-NEXT:         "type": "UserExit",
; CHECK-NEXT:         "payload": "not_inlined"
; CHECK-NEXT:       }
; CHECK-NEXT:     ]
; CHECK-NEXT:   }
; CHECK-NEXT: ]

define void @kernel() !dbg !10 {
  call void @llvm.amdgcn.sqtt.event(metadata !0, i32 poison, i32 0), !dbg !20
  call void @llvm.amdgcn.sqtt.event(metadata !1, i32 poison, i32 0), !dbg !23
  ret void
}

declare void @llvm.amdgcn.sqtt.event(metadata, i32, i32)

!0 = !{i32 2, !"inlined"}
!1 = !{i32 3, !"not_inlined"}

!llvm.dbg.cu = !{!100}
!llvm.module.flags = !{!101, !102}

!100 = distinct !DICompileUnit(language: DW_LANG_C99, file: !103, emissionKind: LineTablesOnly)
!101 = !{i32 2, !"Dwarf Version", i32 5}
!102 = !{i32 2, !"Debug Info Version", i32 3}
!103 = !DIFile(filename: "sqtt-inlined-at.ll", directory: "")
!104 = !DISubroutineType(types: !105)
!105 = !{null}

!10 = distinct !DISubprogram(name: "kernel", scope: !103, file: !103, line: 28, type: !104, spFlags: DISPFlagDefinition, unit: !100)
!11 = distinct !DISubprogram(name: "middle", scope: !103, file: !103, line: 18, type: !104, spFlags: DISPFlagDefinition, unit: !100)
!12 = distinct !DISubprogram(name: "inner", scope: !103, file: !103, line: 8, type: !104, spFlags: DISPFlagDefinition, unit: !100)

!20 = !DILocation(line: 10, column: 3, scope: !12, inlinedAt: !21)
!21 = distinct !DILocation(line: 20, column: 5, scope: !11, inlinedAt: !22)
!22 = distinct !DILocation(line: 30, column: 7, scope: !10)
!23 = !DILocation(line: 31, column: 3, scope: !10)
