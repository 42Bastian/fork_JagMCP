# brew-llvm.cmake
execute_process(COMMAND brew --prefix llvm OUTPUT_VARIABLE LLVM_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_C_COMPILER   "${LLVM_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_PREFIX}/bin/clang++")
set(CMAKE_OBJC_COMPILER "${LLVM_PREFIX}/bin/clang")
# Neither Apple ld (1167) nor lld 23 can synthesize objc_msgSendClass$ stubs yet
set(_objc_nostubs "-fno-objc-msgsend-class-selector-stubs")
set(CMAKE_C_FLAGS_INIT    "${_objc_nostubs}")
set(CMAKE_OBJC_FLAGS_INIT "${_objc_nostubs}")
set(CMAKE_OBJCXX_FLAGS_INIT "${_objc_nostubs}")

