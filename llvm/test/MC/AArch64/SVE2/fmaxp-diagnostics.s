// RUN: not llvm-mc -triple=aarch64 -show-encoding -mattr=+sve2 2>&1 < %s| FileCheck %s

// --------------------------------------------------------------------------//
// Source and Destination Registers must match

fmaxp z0.h, p0/m, z1.h, z2.h
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: operand must match destination register
// CHECK-NEXT: fmaxp z0.h, p0/m, z1.h, z2.h
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:


// --------------------------------------------------------------------------//
// Invalid element width

fmaxp z0.b, p0/m, z0.b, z1.b
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid element width
// CHECK-NEXT: fmaxp z0.b, p0/m, z0.b, z1.b
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:


// --------------------------------------------------------------------------//
// Element sizes must match

fmaxp z0.h, p0/m, z0.s, z1.s
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid element width
// CHECK-NEXT: fmaxp z0.h, p0/m, z0.s, z1.s
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:

fmaxp z0.h, p0/m, z0.h, z1.s
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid element width
// CHECK-NEXT: fmaxp z0.h, p0/m, z0.h, z1.s
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:


// --------------------------------------------------------------------------//
// Invalid predicate operation

fmaxp z0.h, p0/z, z0.h, z1.h
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid operand for instruction
// CHECK-NEXT: fmaxp z0.h, p0/z, z0.h, z1.h
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:


// --------------------------------------------------------------------------//
// Predicate not in restricted predicate range

fmaxp z0.h, p8/m, z0.h, z1.h
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid restricted predicate register, expected p0..p7 (without element suffix)
// CHECK-NEXT: fmaxp z0.h, p8/m, z0.h, z1.h
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:
