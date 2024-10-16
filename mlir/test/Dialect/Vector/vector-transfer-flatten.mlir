// RUN: mlir-opt %s -test-vector-transfer-flatten-patterns -split-input-file | FileCheck %s
// RUN: mlir-opt %s -test-vector-transfer-flatten-patterns=target-vector-bitwidth=128 -split-input-file | FileCheck %s --check-prefix=CHECK-128B

// TODO: Align naming and format with e.g. vector-transfer-permutation-lowering.mlir

///----------------------------------------------------------------------------------------
/// vector.transfer_read
/// [Pattern: FlattenContiguousRowMajorTransferReadPattern]
///----------------------------------------------------------------------------------------

func.func @transfer_read_dims_match_contiguous(
    %arg : memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>) -> vector<5x4x3x2xi8> {

  %c0 = arith.constant 0 : index
  %cst = arith.constant 0 : i8
  %v = vector.transfer_read %arg[%c0, %c0, %c0, %c0], %cst :
    memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>, vector<5x4x3x2xi8>
  return %v : vector<5x4x3x2xi8>
}

// CHECK-LABEL: func @transfer_read_dims_match_contiguous
// CHECK-SAME:    %[[ARG:[0-9a-zA-Z]+]]: memref<5x4x3x2xi8
// CHECK:         %[[COLLAPSED:.+]] = memref.collapse_shape %[[ARG]] {{.}}[0, 1, 2, 3]
// CHECK:         %[[READ1D:.+]] = vector.transfer_read %[[COLLAPSED]]
// CHECK:         %[[VEC2D:.+]] = vector.shape_cast %[[READ1D]] : vector<120xi8> to vector<5x4x3x2xi8>
// CHECK:         return %[[VEC2D]]

// CHECK-128B-LABEL: func @transfer_read_dims_match_contiguous
//       CHECK-128B:   memref.collapse_shape

// -----

func.func @transfer_read_dims_match_contiguous_empty_stride(
    %arg : memref<5x4x3x2xi8>) -> vector<5x4x3x2xi8> {

  %c0 = arith.constant 0 : index
  %cst = arith.constant 0 : i8
  %v = vector.transfer_read %arg[%c0, %c0, %c0, %c0], %cst :
    memref<5x4x3x2xi8>, vector<5x4x3x2xi8>
  return %v : vector<5x4x3x2xi8>
}

// CHECK-LABEL: func @transfer_read_dims_match_contiguous_empty_stride(
// CHECK-SAME:    %[[ARG:[0-9a-zA-Z]+]]: memref<5x4x3x2xi8
// CHECK:         %[[COLLAPSED:.+]] = memref.collapse_shape %[[ARG]] {{.}}[0, 1, 2, 3]
// CHECK:         %[[READ1D:.+]] = vector.transfer_read %[[COLLAPSED]]
// CHECK:         %[[VEC2D:.+]] = vector.shape_cast %[[READ1D]] : vector<120xi8> to vector<5x4x3x2xi8>
// CHECK:         return %[[VEC2D]]

// CHECK-128B-LABEL: func @transfer_read_dims_match_contiguous_empty_stride(
//       CHECK-128B:   memref.collapse_shape

// -----

// The shape of the memref and the vector don't match, but the vector is a
// contiguous subset of the memref, so "flattenable".

func.func @transfer_read_dims_mismatch_contiguous(
    %arg : memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>) -> vector<1x1x2x2xi8> {

  %c0 = arith.constant 0 : index
  %cst = arith.constant 0 : i8
  %v = vector.transfer_read %arg[%c0, %c0, %c0, %c0], %cst :
    memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>, vector<1x1x2x2xi8>
  return %v : vector<1x1x2x2xi8>
}

// CHECK-LABEL:   func.func @transfer_read_dims_mismatch_contiguous(
// CHECK-SAME:      %[[VAL_0:.*]]: memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>) -> vector<1x1x2x2xi8> {
// CHECK:           %[[VAL_1:.*]] = arith.constant 0 : i8
// CHECK:           %[[VAL_2:.*]] = arith.constant 0 : index
// CHECK:           %[[VAL_3:.*]] = memref.collapse_shape %[[VAL_0]] {{\[\[}}0, 1, 2, 3]] : memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>> into memref<120xi8, strided<[1], offset: ?>>
// CHECK:           %[[VAL_4:.*]] = vector.transfer_read %[[VAL_3]]{{\[}}%[[VAL_2]]], %[[VAL_1]] {in_bounds = [true]} : memref<120xi8, strided<[1], offset: ?>>, vector<4xi8>
// CHECK:           %[[VAL_5:.*]] = vector.shape_cast %[[VAL_4]] : vector<4xi8> to vector<1x1x2x2xi8>
// CHECK:           return %[[VAL_5]] : vector<1x1x2x2xi8>

// CHECK-128B-LABEL: func @transfer_read_dims_mismatch_contiguous(
//       CHECK-128B:   memref.collapse_shape

// -----

func.func @transfer_read_dims_mismatch_non_zero_indices(
    %idx_1: index,
    %idx_2: index,
    %arg: memref<1x43x4x6xi32>) -> vector<1x2x6xi32>{

  %c0 = arith.constant 0 : index
  %c0_i32 = arith.constant 0 : i32
  %v = vector.transfer_read %arg[%c0, %idx_1, %idx_2, %c0], %c0_i32 {in_bounds = [true, true, true]} :
    memref<1x43x4x6xi32>, vector<1x2x6xi32>
  return %v : vector<1x2x6xi32>
}

// CHECK: #[[$ATTR_0:.+]] = affine_map<()[s0, s1] -> (s0 * 24 + s1 * 6)>

// CHECK-LABEL:   func.func @transfer_read_dims_mismatch_non_zero_indices(
// CHECK-SAME:      %[[IDX_1:.*]]: index, %[[IDX_2:.*]]: index,
// CHECK-SAME:      %[[M_IN:.*]]: memref<1x43x4x6xi32>
// CHECK:           %[[C_0:.*]] = arith.constant 0 : i32
// CHECK:           %[[C_0_IDX:.*]] = arith.constant 0 : index
// CHECK:           %[[COLLAPSED_IN:.*]] = memref.collapse_shape %[[M_IN]] {{\[}}[0], [1, 2, 3]] : memref<1x43x4x6xi32> into memref<1x1032xi32>
// CHECK:           %[[COLLAPSED_IDX:.*]] = affine.apply #[[$ATTR_0]]()[%[[IDX_1]], %[[IDX_2]]]
// CHECK:           %[[READ:.*]] = vector.transfer_read %[[COLLAPSED_IN]][%[[C_0_IDX]], %[[COLLAPSED_IDX]]], %[[C_0]] {in_bounds = [true]} : memref<1x1032xi32>, vector<12xi32>

// CHECK-128B-LABEL: func @transfer_read_dims_mismatch_non_zero_indices(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

// Overall, the source memref is non-contiguous. However, the slice from which
// the output vector is to be read _is_ contiguous. Hence the flattening works fine.

func.func @transfer_read_dims_mismatch_non_contiguous_non_zero_indices(
    %arg : memref<1x3x3x2xf32, strided<[40, 10, 2, 1], offset: ?>>,
    %idx_1 : index,
    %idx_2 : index) -> vector<2x2xf32> {

  %c0 = arith.constant 0 : index
  %cst_1 = arith.constant 0.000000e+00 : f32
  %8 = vector.transfer_read %arg[%c0, %idx_1, %idx_2, %c0], %cst_1 {in_bounds = [true, true]} :
    memref<1x3x3x2xf32, strided<[40, 10, 2, 1], offset: ?>>, vector<2x2xf32>
  return %8 : vector<2x2xf32>
}

// CHECK: #[[$MAP:.+]] = affine_map<()[s0] -> (s0 * 2)>

// CHECK-LABEL:  func.func @transfer_read_dims_mismatch_non_contiguous_non_zero_indices(
// CHECK:         %[[COLLAPSE:.+]] = memref.collapse_shape %{{.*}} {{\[}}[0], [1], [2, 3]]
// CHECK-SAME:      : memref<1x3x3x2xf32, strided<[40, 10, 2, 1], offset: ?>> into memref<1x3x6xf32, strided<[40, 10, 1], offset: ?>>
// CHECK:         %[[APPLY:.*]] = affine.apply #[[$MAP]]()

// CHECK-128B-LABEL: func @transfer_read_dims_mismatch_non_contiguous_non_zero_indices(
//       CHECK-128B:   memref.collapse_shape

// -----

// The leading dynamic shapes don't affect whether this example is flattenable
// or not. Indeed, those dynamic shapes are not candidates for flattening anyway.

func.func @transfer_read_leading_dynamic_dims(
    %arg : memref<?x?x8x4xi8, strided<[?, 32, 4, 1], offset: ?>>,
    %idx_1 : index,
    %idx_2 : index) -> vector<8x4xi8> {

  %c0_i8 = arith.constant 0 : i8
  %c0 = arith.constant 0 : index
  %result = vector.transfer_read %arg[%idx_1, %idx_2, %c0, %c0], %c0_i8 {in_bounds = [true, true]} :
    memref<?x?x8x4xi8, strided<[?, 32, 4, 1], offset: ?>>, vector<8x4xi8>
  return %result : vector<8x4xi8>
}

// CHECK-LABEL: func @transfer_read_leading_dynamic_dims
// CHECK-SAME:    %[[ARG0:.+]]: memref<?x?x8x4xi8, {{.+}}>, %[[ARG1:.+]]: index, %[[ARG2:.+]]: index
// CHECK:         %[[C0_I8:.+]] = arith.constant 0 : i8
// CHECK:         %[[C0:.+]] = arith.constant 0 : index
// CHECK:         %[[COLLAPSED:.+]] = memref.collapse_shape %[[ARG0]] {{\[}}[0], [1], [2, 3]{{\]}}
// CHECK-SAME:      : memref<?x?x8x4xi8, {{.+}}> into memref<?x?x32xi8, {{.+}}>
// CHECK:         %[[VEC1D:.+]] = vector.transfer_read %[[COLLAPSED]]
// CHECK-SAME:    [%[[ARG1]], %[[ARG2]], %[[C0]]], %[[C0_I8]]
// CHECK-SAME:    {in_bounds = [true]}
// CHECK-SAME:      : memref<?x?x32xi8, {{.+}}>, vector<32xi8>
// CHECK:         %[[VEC2D:.+]] = vector.shape_cast %[[VEC1D]] : vector<32xi8> to vector<8x4xi8>
// CHECK:         return %[[VEC2D]] : vector<8x4xi8>

// CHECK-128B-LABEL: func @transfer_read_leading_dynamic_dims
//       CHECK-128B:   memref.collapse_shape

// -----

// One of the dims to be flattened is dynamic - not supported ATM.

func.func @negative_transfer_read_dynamic_dim_to_flatten(
    %idx_1: index,
    %idx_2: index,
    %m_in: memref<1x?x4x6xi32>) -> vector<1x2x6xi32> {

  %c0 = arith.constant 0 : index
  %c0_i32 = arith.constant 0 : i32
  %v = vector.transfer_read %m_in[%c0, %idx_1, %idx_2, %c0], %c0_i32 {in_bounds = [true, true, true]} :
    memref<1x?x4x6xi32>, vector<1x2x6xi32>
  return %v : vector<1x2x6xi32>
}

// CHECK-LABEL: func.func @negative_transfer_read_dynamic_dim_to_flatten
// CHECK-NOT: memref.collapse_shape
// CHECK-NOT: vector.shape_cast

// CHECK-128B-LABEL: func @negative_transfer_read_dynamic_dim_to_flatten
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

// The vector to be read represents a _non-contiguous_ slice of the input
// memref.

func.func @transfer_read_dims_mismatch_non_contiguous_slice(
    %arg : memref<5x4x3x2xi8>) -> vector<2x1x2x2xi8> {

  %c0 = arith.constant 0 : index
  %cst = arith.constant 0 : i8
  %v = vector.transfer_read %arg[%c0, %c0, %c0, %c0], %cst :
    memref<5x4x3x2xi8>, vector<2x1x2x2xi8>
  return %v : vector<2x1x2x2xi8>
}

// CHECK-LABEL: func.func @transfer_read_dims_mismatch_non_contiguous_slice(
// CHECK-NOT: memref.collapse_shape
// CHECK-NOT: vector.shape_cast

// CHECK-128B-LABEL: func @transfer_read_dims_mismatch_non_contiguous_slice(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

func.func @transfer_read_0d(
    %arg : memref<i8>) -> vector<i8> {

  %cst = arith.constant 0 : i8
  %0 = vector.transfer_read %arg[], %cst : memref<i8>, vector<i8>
  return %0 : vector<i8>
}

// CHECK-LABEL: func.func @transfer_read_0d
// CHECK-NOT: memref.collapse_shape
// CHECK-NOT: vector.shape_cast

// CHECK-128B-LABEL: func @transfer_read_0d(
//   CHECK-128B-NOT:   memref.collapse_shape
//   CHECK-128B-NOT:   vector.shape_cast

// -----

// Strides make the input memref non-contiguous, hence non-flattenable.

func.func @transfer_read_non_contiguous_src(
    %arg : memref<5x4x3x2xi8, strided<[24, 8, 2, 1], offset: ?>>) -> vector<5x4x3x2xi8> {

  %c0 = arith.constant 0 : index
  %cst = arith.constant 0 : i8
  %v = vector.transfer_read %arg[%c0, %c0, %c0, %c0], %cst :
    memref<5x4x3x2xi8, strided<[24, 8, 2, 1], offset: ?>>, vector<5x4x3x2xi8>
  return %v : vector<5x4x3x2xi8>
}

// CHECK-LABEL: func.func @transfer_read_non_contiguous_src
// CHECK-NOT: memref.collapse_shape
// CHECK-NOT: vector.shape_cast

// CHECK-128B-LABEL: func @transfer_read_non_contiguous_src
//   CHECK-128B-NOT:   memref.collapse_shape
//   CHECK-128B-NOT:   vector.shape_cast

// -----

///----------------------------------------------------------------------------------------
/// vector.transfer_write
/// [Pattern: FlattenContiguousRowMajorTransferWritePattern]
///----------------------------------------------------------------------------------------

func.func @transfer_write_dims_match_contiguous(
    %arg : memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>,
    %vec : vector<5x4x3x2xi8>) {

  %c0 = arith.constant 0 : index
  vector.transfer_write %vec, %arg [%c0, %c0, %c0, %c0] :
    vector<5x4x3x2xi8>, memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>
  return
}

// CHECK-LABEL: func @transfer_write_dims_match_contiguous(
// CHECK-SAME:    %[[ARG:[0-9a-zA-Z]+]]: memref<5x4x3x2xi8
// CHECK-SAME:    %[[VEC:[0-9a-zA-Z]+]]: vector<5x4x3x2xi8>
// CHECK-DAG:     %[[COLLAPSED:.+]] = memref.collapse_shape %[[ARG]] {{.}}[0, 1, 2, 3]{{.}} : memref<5x4x3x2xi8, {{.+}}> into memref<120xi8, {{.+}}>
// CHECK-DAG:     %[[VEC1D:.+]] = vector.shape_cast %[[VEC]] : vector<5x4x3x2xi8> to vector<120xi8>
// CHECK:         vector.transfer_write %[[VEC1D]], %[[COLLAPSED]]

// CHECK-128B-LABEL: func @transfer_write_dims_match_contiguous(
//       CHECK-128B:   memref.collapse_shape

// -----

func.func @transfer_write_dims_match_contiguous_empty_stride(
    %arg : memref<5x4x3x2xi8>,
    %vec : vector<5x4x3x2xi8>) {

  %c0 = arith.constant 0 : index
  vector.transfer_write %vec, %arg [%c0, %c0, %c0, %c0] :
    vector<5x4x3x2xi8>, memref<5x4x3x2xi8>
  return
}

// CHECK-LABEL: func @transfer_write_dims_match_contiguous_empty_stride(
// CHECK-SAME:    %[[ARG:[0-9a-zA-Z]+]]: memref<5x4x3x2xi8
// CHECK-SAME:    %[[VEC:[0-9a-zA-Z]+]]: vector<5x4x3x2xi8>
// CHECK-DAG:     %[[COLLAPSED:.+]] = memref.collapse_shape %[[ARG]] {{.}}[0, 1, 2, 3]{{.}} : memref<5x4x3x2xi8> into memref<120xi8>
// CHECK-DAG:     %[[VEC1D:.+]] = vector.shape_cast %[[VEC]] : vector<5x4x3x2xi8> to vector<120xi8>
// CHECK:         vector.transfer_write %[[VEC1D]], %[[COLLAPSED]]

// CHECK-128B-LABEL: func @transfer_write_dims_match_contiguous_empty_stride(
//       CHECK-128B:   memref.collapse_shape

// -----

func.func @transfer_write_dims_mismatch_contiguous(
    %arg : memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>,
    %vec : vector<1x1x2x2xi8>) {

  %c0 = arith.constant 0 : index
  vector.transfer_write %vec, %arg [%c0, %c0, %c0, %c0] :
    vector<1x1x2x2xi8>, memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>
  return
}

// CHECK-LABEL:   func.func @transfer_write_dims_mismatch_contiguous
// CHECK-SAME:      %[[VAL_0:.*]]: memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>,
// CHECK-SAME:      %[[VAL_1:.*]]: vector<1x1x2x2xi8>) {
// CHECK:           %[[VAL_2:.*]] = arith.constant 0 : index
// CHECK:           %[[VAL_3:.*]] = memref.collapse_shape %[[VAL_0]] {{\[\[}}0, 1, 2, 3]] : memref<5x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>> into memref<120xi8, strided<[1], offset: ?>>
// CHECK:           %[[VAL_4:.*]] = vector.shape_cast %[[VAL_1]] : vector<1x1x2x2xi8> to vector<4xi8>
// CHECK:           vector.transfer_write %[[VAL_4]], %[[VAL_3]]{{\[}}%[[VAL_2]]] {in_bounds = [true]} : vector<4xi8>, memref<120xi8, strided<[1], offset: ?>>

// CHECK-128B-LABEL: func @transfer_write_dims_mismatch_contiguous(
//       CHECK-128B:   memref.collapse_shape

// -----

func.func @transfer_write_dims_mismatch_non_zero_indices(
    %idx_1: index,
    %idx_2: index,
    %arg: memref<1x43x4x6xi32>,
    %vec: vector<1x2x6xi32>) {

  %c0 = arith.constant 0 : index
  %c0_i32 = arith.constant 0 : i32
  vector.transfer_write %vec, %arg[%c0, %idx_1, %idx_2, %c0] {in_bounds = [true, true, true]} :
    vector<1x2x6xi32>, memref<1x43x4x6xi32>
  return
}

// CHECK: #[[$ATTR_0:.+]] = affine_map<()[s0, s1] -> (s0 * 24 + s1 * 6)>

// CHECK-LABEL:   func.func @transfer_write_dims_mismatch_non_zero_indices(
// CHECK-SAME:      %[[IDX_1:.*]]: index, %[[IDX_2:.*]]: index,
// CHECK-SAME:      %[[ARG:.*]]: memref<1x43x4x6xi32>,
// CHECK-SAME:      %[[VEC:.*]]: vector<1x2x6xi32>) {
// CHECK-DAG:       %[[C0:.*]] = arith.constant 0 : index
// CHECK-DAG:       %[[IDX:.*]] = affine.apply #[[$ATTR_0]](){{\[}}%[[IDX_1]], %[[IDX_2]]]
// CHECK-DAG:       %[[CS:.*]] = memref.collapse_shape %[[ARG]] {{\[\[}}0], [1, 2, 3]] : memref<1x43x4x6xi32> into memref<1x1032xi32>
// CHECK:           %[[SC:.*]] = vector.shape_cast %[[VEC]] : vector<1x2x6xi32> to vector<12xi32>
// CHECK:           vector.transfer_write %[[SC]], %[[CS]]{{\[}}%[[C0]], %[[IDX]]] {in_bounds = [true]} : vector<12xi32>, memref<1x1032xi32>

// CHECK-128B-LABEL: func @transfer_write_dims_mismatch_non_zero_indices(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

// Overall, the destination memref is non-contiguous. However, the slice to
// which the input vector is to be written _is_ contiguous. Hence the
// flattening works fine.

func.func @transfer_write_dims_mismatch_non_contiguous_non_zero_indices(
    %value : vector<2x2xf32>,
    %subview : memref<1x3x3x2xf32, strided<[40, 10, 2, 1], offset: ?>>,
    %idx_1 : index,
    %idx_2 : index) {

  %c0 = arith.constant 0 : index
  vector.transfer_write %value, %subview[%c0, %idx_1, %idx_2, %c0] {in_bounds = [true, true]} : vector<2x2xf32>, memref<1x3x3x2xf32, strided<[40, 10, 2, 1], offset: ?>>
  return
}

// CHECK:  #[[$MAP:.+]] = affine_map<()[s0] -> (s0 * 2)>

// CHECK-LABEL:  func.func @transfer_write_dims_mismatch_non_contiguous_non_zero_indices(
// CHECK-DAG:      %[[APPLY:.*]] = affine.apply #[[$MAP]]()
// CHECK-DAG:      %[[COLLAPSE:.+]] = memref.collapse_shape %{{.*}} {{\[}}[0], [1], [2, 3]] : memref<1x3x3x2xf32, strided<[40, 10, 2, 1], offset: ?>> into memref<1x3x6xf32, strided<[40, 10, 1], offset: ?>>

// CHECK-128B-LABEL: func @transfer_write_dims_mismatch_non_contiguous_non_zero_indices(
//       CHECK-128B:   memref.collapse_shape

// -----

// The leading dynamic shapes don't affect whether this example is flattenable
// or not. Indeed, those dynamic shapes are not candidates for flattening anyway.

func.func @transfer_write_leading_dynamic_dims(
    %vec : vector<8x4xi8>,
    %arg : memref<?x?x8x4xi8, strided<[?, 32, 4, 1], offset: ?>>,
    %idx_1 : index,
    %idx_2 : index) {

  %c0 = arith.constant 0 : index
  vector.transfer_write %vec, %arg[%idx_1, %idx_2, %c0, %c0] {in_bounds = [true, true]} :
    vector<8x4xi8>, memref<?x?x8x4xi8, strided<[?, 32, 4, 1], offset: ?>>
  return
}

// CHECK-LABEL: func @transfer_write_leading_dynamic_dims
// CHECK-SAME:    %[[ARG0:.+]]: vector<8x4xi8>, %[[ARG1:.+]]: memref<?x?x8x4xi8, {{.+}}>, %[[ARG2:.+]]: index, %[[ARG3:.+]]: index
// CHECK:         %[[C0:.+]] = arith.constant 0 : index
// CHECK:         %[[COLLAPSED:.+]] = memref.collapse_shape %[[ARG1]] {{\[}}[0], [1], [2, 3]{{\]}}
// CHECK-SAME:      : memref<?x?x8x4xi8, {{.+}}> into memref<?x?x32xi8, {{.+}}>
// CHECK:         %[[VEC1D:.+]] = vector.shape_cast %[[ARG0]] : vector<8x4xi8> to vector<32xi8>
// CHECK:         vector.transfer_write %[[VEC1D]], %[[COLLAPSED]]
// CHECK-SAME:      [%[[ARG2]], %[[ARG3]], %[[C0]]]
// CHECK-SAME:      {in_bounds = [true]}
// CHECK-SAME:      : vector<32xi8>, memref<?x?x32xi8, {{.+}}>

// CHECK-128B-LABEL: func @transfer_write_leading_dynamic_dims
//       CHECK-128B:   memref.collapse_shape

// -----

// One of the dims to be flattened is dynamic - not supported ATM.

func.func @negative_transfer_write_dynamic_to_flatten(
    %idx_1: index,
    %idx_2: index,
    %vec : vector<1x2x6xi32>,
    %arg: memref<1x?x4x6xi32>) {

  %c0 = arith.constant 0 : index
  %c0_i32 = arith.constant 0 : i32
  vector.transfer_write %vec, %arg[%c0, %idx_1, %idx_2, %c0] {in_bounds = [true, true, true]} :
    vector<1x2x6xi32>, memref<1x?x4x6xi32>
  return
}

// CHECK-LABEL: func.func @negative_transfer_write_dynamic_to_flatten
// CHECK-NOT: memref.collapse_shape
// CHECK-NOT: vector.shape_cast

// CHECK-128B-LABEL: func @negative_transfer_write_dynamic_to_flatten
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

// The vector to be written represents a _non-contiguous_ slice of the output
// memref.

func.func @transfer_write_dims_mismatch_non_contiguous_slice(
    %arg : memref<5x4x3x2xi8>,
    %vec : vector<2x1x2x2xi8>) {

  %c0 = arith.constant 0 : index
  %cst = arith.constant 0 : i8
  vector.transfer_write %vec, %arg[%c0, %c0, %c0, %c0] :
    vector<2x1x2x2xi8>, memref<5x4x3x2xi8>
  return
}

// CHECK-LABEL: func.func @transfer_write_dims_mismatch_non_contiguous_slice(
// CHECK-NOT: memref.collapse_shape
// CHECK-NOT: vector.shape_cast

// CHECK-128B-LABEL: func @transfer_write_dims_mismatch_non_contiguous_slice(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

func.func @transfer_write_0d(
    %arg : memref<i8>,
    %vec : vector<i8>) {

  vector.transfer_write %vec, %arg[] : vector<i8>, memref<i8>
  return
}

// CHECK-LABEL: func.func @transfer_write_0d
// CHECK-NOT: memref.collapse_shape
// CHECK-NOT: vector.shape_cast

// CHECK-128B-LABEL: func @transfer_write_0d(
//   CHECK-128B-NOT:   memref.collapse_shape
//   CHECK-128B-NOT:   vector.shape_cast

// -----

// The strides make the input memref non-contiguous, hence non-flattenable.

func.func @transfer_write_non_contiguous_src(
    %arg : memref<5x4x3x2xi8, strided<[24, 8, 2, 1], offset: ?>>,
    %vec : vector<5x4x3x2xi8>) {

  %c0 = arith.constant 0 : index
  vector.transfer_write %vec, %arg[%c0, %c0, %c0, %c0] :
   vector<5x4x3x2xi8>, memref<5x4x3x2xi8, strided<[24, 8, 2, 1], offset: ?>>
  return
}

// CHECK-LABEL: func.func @transfer_write_non_contiguous_src
// CHECK-NOT: memref.collapse_shape
// CHECK-NOT: vector.shape_cast

// CHECK-128B-LABEL: func @transfer_write_non_contiguous_src
//   CHECK-128B-NOT:   memref.collapse_shape
//   CHECK-128B-NOT:   vector.shape_cast

// -----

///----------------------------------------------------------------------------------------
/// [Pattern: DropUnitDimFromElementwiseOps]
/// TODO: Move to a dedicated file - there's no "flattening" in the following tests
///----------------------------------------------------------------------------------------

func.func @fold_unit_dim_add_basic(%arg0 : vector<1x8xi32>) -> vector<1x8xi32> {
   %add = arith.addi %arg0, %arg0 : vector<1x8xi32>
   return %add : vector<1x8xi32>
}
// CHECK-LABEL:   func.func @fold_unit_dim_add_basic(
// CHECK-SAME:      %[[VAL_0:.*]]: vector<1x8xi32>) -> vector<1x8xi32> {
// CHECK:           %[[VAL_1:.*]] = vector.shape_cast %[[VAL_0]] : vector<1x8xi32> to vector<8xi32>
// CHECK:           %[[VAL_2:.*]] = vector.shape_cast %[[VAL_0]] : vector<1x8xi32> to vector<8xi32>
// CHECK:           %[[VAL_3:.*]] = arith.addi %[[VAL_1]], %[[VAL_2]] : vector<8xi32>
// CHECK:           %[[VAL_4:.*]] = vector.shape_cast %[[VAL_3]] : vector<8xi32> to vector<1x8xi32>
// CHECK:           return %[[VAL_4]] : vector<1x8xi32>

// CHECK-128B-LABEL: func @fold_unit_dim_add_basic(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

func.func @fold_unit_dim_add_leading_and_trailing(%arg0 : vector<1x8x1xi32>) -> vector<1x8x1xi32> {
   %add = arith.addi %arg0, %arg0 : vector<1x8x1xi32>
   return %add : vector<1x8x1xi32>
}
// CHECK-LABEL:   func.func @fold_unit_dim_add_leading_and_trailing(
// CHECK-SAME:      %[[VAL_0:.*]]: vector<1x8x1xi32>) -> vector<1x8x1xi32> {
// CHECK:           %[[VAL_1:.*]] = vector.shape_cast %[[VAL_0]] : vector<1x8x1xi32> to vector<8xi32>
// CHECK:           %[[VAL_2:.*]] = vector.shape_cast %[[VAL_0]] : vector<1x8x1xi32> to vector<8xi32>
// CHECK:           %[[VAL_3:.*]] = arith.addi %[[VAL_1]], %[[VAL_2]] : vector<8xi32>
// CHECK:           %[[VAL_4:.*]] = vector.shape_cast %[[VAL_3]] : vector<8xi32> to vector<1x8x1xi32>
// CHECK:           return %[[VAL_4]] : vector<1x8x1xi32>

// CHECK-128B-LABEL: func @fold_unit_dim_add_leading_and_trailing(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

func.func @fold_unit_dim_add(%arg0 : vector<8x1xi32>,
                             %arg1 : vector<1x8xi32>) -> vector<8xi32> {
   %sc_arg0 = vector.shape_cast %arg0 : vector<8x1xi32> to vector<1x8xi32>
   %add = arith.addi %sc_arg0, %arg1 : vector<1x8xi32>
   %res = vector.shape_cast %add : vector<1x8xi32> to vector<8xi32>
   return %res : vector<8xi32>
}

// CHECK-LABEL:   func.func @fold_unit_dim_add(
// CHECK-SAME:      %[[VAL_0:.*]]: vector<8x1xi32>,
// CHECK-SAME:      %[[VAL_1:.*]]: vector<1x8xi32>) -> vector<8xi32> {
// CHECK:           %[[VAL_2:.*]] = vector.shape_cast %[[VAL_0]] : vector<8x1xi32> to vector<8xi32>
// CHECK:           %[[VAL_3:.*]] = vector.shape_cast %[[VAL_1]] : vector<1x8xi32> to vector<8xi32>
// CHECK:           %[[VAL_4:.*]] = arith.addi %[[VAL_2]], %[[VAL_3]] : vector<8xi32>
// CHECK:           return %[[VAL_4]] : vector<8xi32>

// CHECK-128B-LABEL: func @fold_unit_dim_add(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

func.func @fold_unit_dim_mulf(%arg0 : vector<8x[2]x1xf32>,
                              %arg1 : vector<1x8x[2]xf32>) -> vector<8x[2]xf32> {
   %sc_arg0 = vector.shape_cast %arg0 : vector<8x[2]x1xf32> to vector<1x8x[2]xf32>
   %add = arith.mulf %sc_arg0, %arg1 : vector<1x8x[2]xf32>
   %res = vector.shape_cast %add : vector<1x8x[2]xf32> to vector<8x[2]xf32>
   return %res : vector<8x[2]xf32>
}

// CHECK-LABEL:   func.func @fold_unit_dim_mulf(
// CHECK-SAME:      %[[VAL_0:.*]]: vector<8x[2]x1xf32>,
// CHECK-SAME:      %[[VAL_1:.*]]: vector<1x8x[2]xf32>) -> vector<8x[2]xf32> {
// CHECK:           %[[VAL_2:.*]] = vector.shape_cast %[[VAL_0]] : vector<8x[2]x1xf32> to vector<8x[2]xf32>
// CHECK:           %[[VAL_3:.*]] = vector.shape_cast %[[VAL_1]] : vector<1x8x[2]xf32> to vector<8x[2]xf32>
// CHECK:           %[[VAL_4:.*]] = arith.mulf %[[VAL_2]], %[[VAL_3]] : vector<8x[2]xf32>
// CHECK:           return %[[VAL_4]] : vector<8x[2]xf32>

// CHECK-128B-LABEL: func @fold_unit_dim_mulf(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

func.func @fold_unit_dim_sitofp(%arg0 : vector<8x[2]x1xi8>) -> vector<8x[2]xf32> {
   %sc_arg0 = vector.shape_cast %arg0 : vector<8x[2]x1xi8> to vector<1x8x[2]xi8>
   %add = arith.sitofp %sc_arg0 : vector<1x8x[2]xi8> to vector<1x8x[2]xf32>
   %res = vector.shape_cast %add : vector<1x8x[2]xf32> to vector<8x[2]xf32>
   return %res : vector<8x[2]xf32>
}

// CHECK-LABEL:   func.func @fold_unit_dim_sitofp(
// CHECK-SAME:      %[[VAL_0:.*]]: vector<8x[2]x1xi8>) -> vector<8x[2]xf32> {
// CHECK:           %[[VAL_1:.*]] = vector.shape_cast %[[VAL_0]] : vector<8x[2]x1xi8> to vector<8x[2]xi8>
// CHECK:           %[[VAL_2:.*]] = arith.sitofp %[[VAL_1]] : vector<8x[2]xi8> to vector<8x[2]xf32>
// CHECK:           return %[[VAL_2]] : vector<8x[2]xf32>

// CHECK-128B-LABEL: func @fold_unit_dim_sitofp(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

// All shape casts are folded away

func.func @fold_unit_dims_entirely(%arg0 : vector<8xi32>,
                                   %arg1 : vector<8xi32>,
                                   %arg2 : vector<8xi32>) -> vector<8xi32> {
   %sc_arg0 = vector.shape_cast %arg0 : vector<8xi32> to vector<1x8xi32>
   %sc_arg1 = vector.shape_cast %arg1 : vector<8xi32> to vector<1x8xi32>
   %sc_arg2 = vector.shape_cast %arg2 : vector<8xi32> to vector<1x8xi32>
   %mul = arith.muli %sc_arg0, %sc_arg1 : vector<1x8xi32>
   %add = arith.addi %mul, %sc_arg2 : vector<1x8xi32>
   %res = vector.shape_cast %add : vector<1x8xi32> to vector<8xi32>
   return %res : vector<8xi32>
}

// CHECK-LABEL:   func.func @fold_unit_dims_entirely(
// CHECK-SAME:      %[[VAL_0:.*]]: vector<8xi32>, %[[VAL_1:.*]]: vector<8xi32>,
// CHECK-SAME:      %[[VAL_2:.*]]: vector<8xi32>) -> vector<8xi32> {
// CHECK:           %[[VAL_3:.*]] = arith.muli %[[VAL_0]], %[[VAL_1]] : vector<8xi32>
// CHECK:           %[[VAL_4:.*]] = arith.addi %[[VAL_3]], %[[VAL_2]] : vector<8xi32>
// CHECK:           return %[[VAL_4]] : vector<8xi32>

// CHECK-128B-LABEL: func @fold_unit_dims_entirely(
//   CHECK-128B-NOT:   memref.collapse_shape

// -----

func.func @fold_inner_unit_dim(%arg0 : vector<8x1x3xf128>,
                              %arg1 : vector<1x8x3xf128>) -> vector<8x3xf128> {
   %sc_arg1 = vector.shape_cast %arg1 : vector<1x8x3xf128> to vector<8x1x3xf128>
   %mul = arith.mulf %arg0, %sc_arg1 : vector<8x1x3xf128>
   %res = vector.shape_cast %mul : vector<8x1x3xf128> to vector<8x3xf128>
   return %res : vector<8x3xf128>
}

// CHECK-LABEL: func.func @fold_inner_unit_dim(
// CHECK-SAME:    %[[VAL_0:.*]]: vector<8x1x3xf128>,
// CHECK-SAME:    %[[VAL_1:.*]]: vector<1x8x3xf128>) -> vector<8x3xf128> {
// CHECK:         %[[VAL_2:.*]] = vector.shape_cast %[[VAL_0]] : vector<8x1x3xf128> to vector<8x3xf128>
// CHECK:         %[[VAL_3:.*]] = vector.shape_cast %[[VAL_1]] : vector<1x8x3xf128> to vector<8x3xf128>
// CHECK:         %[[VAL_4:.*]] = arith.mulf %[[VAL_2]], %[[VAL_3]] : vector<8x3xf128>
// CHECK:         return %[[VAL_4]] : vector<8x3xf128>

// -----

func.func @fold_inner_unit_dim_scalable(%arg0 : vector<8x1x[1]x3xf128>,
                              %arg1 : vector<1x8x[1]x3xf128>) -> vector<8x[1]x3xf128> {
   %sc_arg1 = vector.shape_cast %arg1 : vector<1x8x[1]x3xf128> to vector<8x1x[1]x3xf128>
   %mul = arith.mulf %arg0, %sc_arg1 : vector<8x1x[1]x3xf128>
   %res = vector.shape_cast %mul : vector<8x1x[1]x3xf128> to vector<8x[1]x3xf128>
   return %res : vector<8x[1]x3xf128>
}

// CHECK-LABEL: func.func @fold_inner_unit_dim_scalable(
// CHECK-SAME:    %[[VAL_0:.*]]: vector<8x1x[1]x3xf128>,
// CHECK-SAME:    %[[VAL_1:.*]]: vector<1x8x[1]x3xf128>) -> vector<8x[1]x3xf128> {
// CHECK:         %[[VAL_2:.*]] = vector.shape_cast %[[VAL_0]] : vector<8x1x[1]x3xf128> to vector<8x[1]x3xf128>
// CHECK:         %[[VAL_3:.*]] = vector.shape_cast %[[VAL_1]] : vector<1x8x[1]x3xf128> to vector<8x[1]x3xf128>
// CHECK:         %[[VAL_4:.*]] = arith.mulf %[[VAL_2]], %[[VAL_3]] : vector<8x[1]x3xf128>
// CHECK:         return %[[VAL_4]] : vector<8x[1]x3xf128>

// -----

func.func @fold_all_unit_dims(%arg0: vector<1x1xf32>) -> vector<1xf32> {
  %0 = arith.mulf %arg0, %arg0 : vector<1x1xf32>
  %res = vector.shape_cast %0 : vector<1x1xf32> to vector<1xf32>
  return %res : vector<1xf32>
}

// CHECK-LABEL: func.func @fold_all_unit_dims(
// CHECK-SAME:    %[[VAL_0:.*]]: vector<1x1xf32>) -> vector<1xf32>
// CHECK:         %[[VAL_1:.*]] = vector.shape_cast %[[VAL_0]] : vector<1x1xf32> to vector<1xf32>
// CHECK:         %[[VAL_2:.*]] = vector.shape_cast %[[VAL_0]] : vector<1x1xf32> to vector<1xf32>
// CHECK:         %[[VAL_3:.*]] = arith.mulf %[[VAL_1]], %[[VAL_2]] : vector<1xf32>
// CHECK:         return %[[VAL_3]] : vector<1xf32>

// -----

func.func @negative_out_of_bound_transfer_read(
    %arg : memref<?x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>) -> vector<5x4x3x2xi8> {
  %c0 = arith.constant 0 : index
  %cst = arith.constant 0 : i8
  %v = vector.transfer_read %arg[%c0, %c0, %c0, %c0], %cst {in_bounds = [false, true, true, true]} :
    memref<?x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>, vector<5x4x3x2xi8>
  return %v : vector<5x4x3x2xi8>
}
// CHECK:     func.func @negative_out_of_bound_transfer_read
// CHECK-NOT:   memref.collapse_shape

// -----

func.func @negative_out_of_bound_transfer_write(
    %arg : memref<?x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>, %vec : vector<1x1x3x2xi8>) {
  %c0 = arith.constant 0 : index
  vector.transfer_write %vec, %arg [%c0, %c0, %c0, %c0] {in_bounds = [false, true, true, true]} :
    vector<1x1x3x2xi8>, memref<?x4x3x2xi8, strided<[24, 6, 2, 1], offset: ?>>
  return
}
// CHECK:     func.func @negative_out_of_bound_transfer_write
// CHECK-NOT:   memref.collapse_shape
