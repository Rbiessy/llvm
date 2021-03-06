add_custom_target(SYCLUnitTests)
set_target_properties(SYCLUnitTests PROPERTIES FOLDER "SYCL tests")

# add_sycl_unittest(test_dirname file1.cpp, file2.cpp ...)
#
# Will compile the list of files together and link against SYCL.
# Produces a binary names `basename(test_dirname)`.
function(add_sycl_unittest test_dirname)
  # Enable exception handling for these unit tests
  set(LLVM_REQUIRES_EH 1)

  if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
    set(sycl_lib "sycld")
  else()
    set(sycl_lib "sycl")
  endif()

  add_unittest(SYCLUnitTests ${test_dirname} ${ARGN})
  target_link_libraries(${test_dirname}
    PRIVATE
      ${sycl_lib}
      LLVMTestingSupport
      OpenCL-Headers
    )
  target_include_directories(${test_dirname} PRIVATE SYSTEM
      ${sycl_inc_dir}
      ${SYCL_SOURCE_DIR}/source/
    )
  # LLVM gtest uses LLVM utilities that require C++-14
  # CXX_STANDARD_REQUIRED makes CXX_STANDARD a hard requirement.
  set_target_properties(${test_dirname}
    PROPERTIES
      CXX_STANDARD 14
      CXX_STANDARD_REQUIRED ON
    )
endfunction()

add_subdirectory(pi)
add_subdirectory(misc)
add_subdirectory(thread_safety)
add_subdirectory(scheduler)
