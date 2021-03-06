// RUN: %clang %s -fsyntax-only -fsycl-device-only -DTRIGGER_ERROR -Xclang -verify
// RUN: %clang %s -fsyntax-only -Xclang -ast-dump -fsycl-device-only | FileCheck %s
// RUN: %clang_cc1 -fsycl-is-host -fsyntax-only -verify %s

#ifndef __SYCL_DEVICE_ONLY__
struct FuncObj {
  [[intelfpga::num_simd_work_items(42)]] // expected-no-diagnostics
  void operator()() {}
};

template <typename name, typename Func>
void kernel(Func kernelFunc) {
  kernelFunc();
}

void foo() {
  kernel<class test_kernel1>(
      FuncObj());
}

#else // __SYCL_DEVICE_ONLY__

[[intelfpga::num_simd_work_items(2)]] // expected-warning{{'num_simd_work_items' attribute ignored}}
void func_ignore() {}

struct FuncObj {
  [[intelfpga::num_simd_work_items(42)]]
  void operator()() {}
};

template <typename name, typename Func>
__attribute__((sycl_kernel)) void kernel(Func kernelFunc) {
  kernelFunc();
}

int main() {
  // CHECK-LABEL: FunctionDecl {{.*}} _ZTSZ4mainE12test_kernel1
  // CHECK:       SYCLIntelNumSimdWorkItemsAttr {{.*}} 42
  kernel<class test_kernel1>(
      FuncObj());

  // CHECK-LABEL: FunctionDecl {{.*}} _ZTSZ4mainE12test_kernel2
  // CHECK:       SYCLIntelNumSimdWorkItemsAttr {{.*}} 8
  kernel<class test_kernel2>(
      []() [[intelfpga::num_simd_work_items(8)]] {});

  // CHECK-LABEL: FunctionDecl {{.*}} _ZTSZ4mainE12test_kernel3
  // CHECK-NOT:   SYCLIntelNumSimdWorkItemsAttr {{.*}} 2
  kernel<class test_kernel3>(
      []() {func_ignore();});

#ifdef TRIGGER_ERROR
  [[intelfpga::num_simd_work_items(0)]] int Var = 0; // expected-error{{'num_simd_work_items' attribute only applies to functions}}

  kernel<class test_kernel4>(
      []() [[intelfpga::num_simd_work_items(0)]] {}); // expected-error{{'num_simd_work_items' attribute must be greater than 0}}

  kernel<class test_kernel5>(
      []() [[intelfpga::num_simd_work_items(-42)]] {}); // expected-error{{'num_simd_work_items' attribute requires a non-negative integral compile time constant expression}}

  kernel<class test_kernel6>(
      []() [[intelfpga::num_simd_work_items(1), intelfpga::num_simd_work_items(2)]] {}); // expected-warning{{attribute 'num_simd_work_items' is already applied with different parameters}}
#endif // TRIGGER_ERROR
}
#endif // __SYCL_DEVICE_ONLY__
