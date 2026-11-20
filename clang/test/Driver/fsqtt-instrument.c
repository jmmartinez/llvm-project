// HIP host compilation does not get SQTT instrumentation.
// RUN: %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=ON,HIP %s
// ON:         "-D__SQTT_INSTRUMENT__"
// ON-SAME:    "-fsqtt-instrument=before-inlining"
// HIP:        "-cc1" "-triple" "x86_64-unknown-linux-gnu"
// HIP-NOT:    "-D__SQTT_INSTRUMENT__"
// HIP-NOT:    "-fsqtt-instrument=

// C, C++, and OpenCL use the same instrumentation as HIP device.
// RUN: %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument %s 2>&1 | FileCheck --check-prefix=ON %s
// RUN: %clang -### -x c++ -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument %s 2>&1 | FileCheck --check-prefix=ON %s
// RUN: %clang -### -x cl -c --target=amdgcn-amd-amdhsa -nogpuinc -nogpulib \
// RUN:   -fsqtt-instrument %s 2>&1 | FileCheck --check-prefix=ON %s

// Without the option there is neither instrumentation nor a predefined macro.
// RUN: %clang -### --target=amdgcn-amd-amdhsa -mcpu=gfx900 -nogpulib %s 2>&1 \
// RUN:   | FileCheck --check-prefix=OFF %s
// OFF-NOT: __SQTT_INSTRUMENT__
// OFF-NOT: "-fsqtt-instrument=

// SQTT instrumentation does not interact with -finstrument-functions.
// RUN: %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument -finstrument-functions %s 2>&1 \
// RUN:   | FileCheck --check-prefix=BOTH %s
// BOTH: "-fsqtt-instrument=before-inlining"
// BOTH-SAME: "-finstrument-functions"

// The variant that instruments after inlining. -fsqtt-instrument is an alias of
// -fsqtt-instrument=before-inlining.
// RUN: %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument=after-inlining %s 2>&1 \
// RUN:   | FileCheck --check-prefix=AFTER %s
// RUN: %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument=after-inlining %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=AFTER,AFTER-HIP %s
// AFTER:           "-D__SQTT_INSTRUMENT__"
// AFTER-SAME:      "-fsqtt-instrument=after-inlining"
// AFTER-HIP:       "-cc1" "-triple" "x86_64-unknown-linux-gnu"
// AFTER-HIP-NOT:   "-D__SQTT_INSTRUMENT__"
// AFTER-HIP-NOT:   "-fsqtt-instrument=

// Only one kind of instrumentation is enabled: the last option wins.
// RUN: %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument -fsqtt-instrument=after-inlining %s 2>&1 \
// RUN:   | FileCheck --check-prefix=AFTER %s
// RUN: %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument=after-inlining -fsqtt-instrument %s 2>&1 \
// RUN:   | FileCheck --check-prefix=ON %s

// -fsqtt-instrument=none disables the instrumentation.
// RUN: %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument -fsqtt-instrument=none %s 2>&1 \
// RUN:   | FileCheck --check-prefix=OFF %s
