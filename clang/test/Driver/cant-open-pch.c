// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: %clang -x c-header %S/Inputs/pchfile.h -o %t/pchfile.h.pch
// RUN: cd %t

// It can access the PCH
// RUN: %clang %s -I %S/Inputs -include pchfile.h -### 2>&1 | FileCheck %s -check-prefix=CHECK-INCLUDE-PCH
// CHECK-INCLUDE-PCH: "-include-pch" "pchfile.h.pch"

// It cannot access the PCH
// RUN: chmod -r %t/pchfile.h.pch
// RUN: %clang %s -I %S/Inputs -include pchfile.h -### 2>&1 | FileCheck %s -check-prefix=CHECK-INCLUDE-PCH-FAIL
// CHECK-INCLUDE-PCH-FAIL: precompiled header 'pchfile.h.pch' was ignored because clang does not have read access to it
// CHECK-INCLUDE-PCH-FAIL-NOT: "-include-pch"
// CHECK-INCLUDE-PCH-FAIL: "-include" "pchfile.h"
