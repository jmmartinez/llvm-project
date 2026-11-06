// HIP host compilation does not get SQTT instrumentation.
// RUN: %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=PRE,HIP-PRE %s
// RUN: %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument=before-inlining %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=PRE,HIP-PRE %s
// PRE:          "-D__SQTT_INSTRUMENT__"
// PRE-SAME:     "-finstrument-functions"
// PRE-SAME:     "-finstrument-function-prefix=sqtt"
// HIP-PRE:      "-cc1" "-triple" "x86_64-unknown-linux-gnu"
// HIP-PRE-NOT:  "-D__SQTT_INSTRUMENT__"
// HIP-PRE-NOT:  "-finstrument-functions"
// HIP-PRE-NOT:  "-finstrument-function-prefix=sqtt"

// RUN: %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument=after-inlining %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=POST,HIP-POST %s
// POST:          "-D__SQTT_INSTRUMENT__"
// POST-SAME:     "-finstrument-functions-after-inlining"
// POST-SAME:     "-finstrument-function-prefix=sqtt"
// HIP-POST:      "-cc1" "-triple" "x86_64-unknown-linux-gnu"
// HIP-POST-NOT:  "-D__SQTT_INSTRUMENT__"
// HIP-POST-NOT:  "-finstrument-functions"
// HIP-POST-NOT:  "-finstrument-function-prefix=sqtt"

// C, C++, and OpenCL use the same instrumentation as HIP device.
// RUN: %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument %s 2>&1 | FileCheck --check-prefix=PRE %s
// RUN: %clang -### -x c++ -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument %s 2>&1 | FileCheck --check-prefix=PRE %s
// RUN: %clang -### -x cl -c --target=amdgcn-amd-amdhsa -nogpuinc -nogpulib \
// RUN:   -fsqtt-instrument %s 2>&1 | FileCheck --check-prefix=PRE %s
// RUN: %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument=after-inlining %s 2>&1 | FileCheck --check-prefix=POST %s
// RUN: %clang -### -x cl -c --target=amdgcn-amd-amdhsa -nogpuinc -nogpulib \
// RUN:   -fsqtt-instrument=after-inlining %s 2>&1 | FileCheck --check-prefix=POST %s

// Without the option there is neither instrumentation nor a predefined macro.
// RUN: %clang -### --target=amdgcn-amd-amdhsa -mcpu=gfx900 -nogpulib %s 2>&1 \
// RUN:   | FileCheck --check-prefix=OFF %s
// OFF-NOT: __SQTT_INSTRUMENT__
// OFF-NOT: "-finstrument-function-prefix=sqtt"

// SQTT instrumentation conflicts with other instrumentation.
// RUN: not %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument -finstrument-functions %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CONFLICT-PRE %s
// RUN: not %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument -finstrument-functions %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CONFLICT-PRE %s
// CONFLICT-PRE: error: invalid argument '-fsqtt-instrument' not allowed with '-finstrument-functions'

// RUN: not %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument=after-inlining \
// RUN:   -finstrument-functions-after-inlining %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CONFLICT-POST %s
// CONFLICT-POST: error: invalid argument '-fsqtt-instrument=after-inlining' not allowed with '-finstrument-functions-after-inlining'

// RUN: not %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument -finstrument-function-entry-bare %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CONFLICT-BARE %s
// CONFLICT-BARE: error: invalid argument '-fsqtt-instrument' not allowed with '-finstrument-function-entry-bare'

// Invalid SQTT option value.
// RUN: not %clang -### -x hip -nogpuinc -nogpulib --target=x86_64-unknown-linux-gnu \
// RUN:   --offload-arch=gfx906 -fsqtt-instrument=wrong %s 2>&1 \
// RUN:   | FileCheck --check-prefix=INVALID %s
// RUN: not %clang -### -x c -c --target=x86_64-unknown-linux-gnu \
// RUN:   -fsqtt-instrument=wrong %s 2>&1 | FileCheck --check-prefix=INVALID %s
// INVALID: error: invalid value 'wrong' in '-fsqtt-instrument=wrong'
