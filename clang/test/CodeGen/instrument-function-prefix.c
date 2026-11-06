// RUN: %clang_cc1 -emit-llvm -o - %s -finstrument-function-prefix=custom -disable-llvm-passes | FileCheck -check-prefix=NONE %s
// RUN: %clang_cc1 -emit-llvm -o - %s -finstrument-functions -finstrument-function-prefix=custom -disable-llvm-passes | FileCheck -check-prefix=CUSTOM %s
// RUN: %clang_cc1 -emit-llvm -o - %s -finstrument-functions-after-inlining -finstrument-function-prefix=custom -disable-llvm-passes | FileCheck -check-prefix=AFTER %s
// RUN: %clang_cc1 -emit-llvm -o - %s -finstrument-function-entry-bare -finstrument-function-prefix=custom -disable-llvm-passes | FileCheck -check-prefix=BARE %s

int test1(int x) {
// CUSTOM: @test1(i32 {{.*}}%x) #[[ATTR1:[0-9]+]]
// AFTER: @test1(i32 {{.*}}%x) #[[ATTR1:[0-9]+]]
// BARE: @test1(i32 {{.*}}%x) #[[ATTR1:[0-9]+]]
  return x;
}

int test2(int) __attribute__((no_instrument_function));
int test2(int x) {
// CUSTOM: @test2(i32 {{.*}}%x) #[[ATTR2:[0-9]+]]
// AFTER: @test2(i32 {{.*}}%x) #[[ATTR2:[0-9]+]]
// BARE: @test2(i32 {{.*}}%x) #[[ATTR2:[0-9]+]]
  return x;
}

// CUSTOM: attributes #[[ATTR1]] =
// CUSTOM-SAME: "instrument-function-entry"="custom_func_enter"
// CUSTOM-SAME: "instrument-function-exit"="custom_func_exit"

// AFTER: attributes #[[ATTR1]] =
// AFTER-SAME: "instrument-function-entry-inlined"="custom_func_enter"
// AFTER-SAME: "instrument-function-exit-inlined"="custom_func_exit"

// BARE: attributes #[[ATTR1]] =
// BARE-SAME: "instrument-function-entry-inlined"="custom_func_enter_bare"

// CUSTOM: attributes #[[ATTR2]] =
// CUSTOM-NOT: "instrument-function-entry"

// AFTER: attributes #[[ATTR2]] =
// AFTER-NOT: "instrument-function-entry"

// BARE: attributes #[[ATTR2]] =
// BARE-NOT: "instrument-function-entry"

// NONE-NOT: "instrument-function-entry"
// NONE-NOT: "instrument-function-exit"
