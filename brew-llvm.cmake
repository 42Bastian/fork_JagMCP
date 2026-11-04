# brew-llvm.cmake
execute_process(COMMAND brew --prefix llvm OUTPUT_VARIABLE LLVM_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_C_COMPILER   "${LLVM_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_PREFIX}/bin/clang++")
set(CMAKE_OBJC_COMPILER "${LLVM_PREFIX}/bin/clang")
# CMake would cache these tools by the Cellar's versioned path, which an upgrade of llvm removes
foreach(lang C CXX OBJC OBJCXX)
  set(CMAKE_${lang}_COMPILER_AR "${LLVM_PREFIX}/bin/llvm-ar" CACHE FILEPATH "" FORCE)
  set(CMAKE_${lang}_COMPILER_RANLIB "${LLVM_PREFIX}/bin/llvm-ranlib" CACHE FILEPATH "" FORCE)
endforeach()
# Neither Apple ld (1167) nor lld 23 can synthesize objc_msgSendClass$ stubs yet
set(_objc_nostubs "-fno-objc-msgsend-class-selector-stubs")
set(CMAKE_C_FLAGS_INIT    "${_objc_nostubs}")
set(CMAKE_OBJC_FLAGS_INIT "${_objc_nostubs}")
set(CMAKE_OBJCXX_FLAGS_INIT "${_objc_nostubs}")

