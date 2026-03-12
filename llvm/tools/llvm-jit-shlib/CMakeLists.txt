# Builds libKNHR.dylib (Kotlin/Native Hot Reload) — a shared library containing
# the LLVM ORC JIT and JITLink components needed by Kotlin/Native hot reload.
#
# This is included in the "essentials" LLVM distribution so that end-user
# compilers can link the hot reload host executable without shipping the full
# set of LLVM static libraries.
#
# Enable with: -DLLVM_BUILD_KOTLIN_JIT_DYLIB=ON

option(LLVM_BUILD_KOTLIN_JIT_DYLIB
  "Build libKNHR shared library for Kotlin/Native hot reload" OFF)

if(NOT LLVM_BUILD_KOTLIN_JIT_DYLIB)
  return()
endif()

# JIT core components.

set(KOTLIN_JIT_COMPONENTS
  OrcJIT
  OrcDebugging
  OrcShared
  OrcTargetProcess
  JITLink
  ExecutionEngine
  RuntimeDyld
  # Required by OrcJIT transitive deps
  Passes
  IRReader
)

# Conditionally add target backends based on what LLVM was configured with.
# AArch64 covers macOS ARM64, iOS device, iOS Simulator on Apple Silicon.
# X86 covers macOS x86_64, iOS Simulator on Intel.
foreach(target IN ITEMS AArch64 X86)
  if("${target}" IN_LIST LLVM_TARGETS_TO_BUILD)
    list(APPEND KOTLIN_JIT_COMPONENTS
      ${target}CodeGen
      ${target}AsmParser
      ${target}Desc
      ${target}Disassembler
      ${target}Info
    )
    if("${target}" STREQUAL "AArch64")
      list(APPEND KOTLIN_JIT_COMPONENTS AArch64Utils)
    endif()
  endif()
endforeach()

llvm_map_components_to_libnames(JIT_LIB_NAMES ${KOTLIN_JIT_COMPONENTS})
list(REMOVE_DUPLICATES JIT_LIB_NAMES)

add_llvm_library(KNHR SHARED
  DISABLE_LLVM_LINK_LLVM_DYLIB
  OUTPUT_NAME KNHR
  INSTALL_WITH_TOOLCHAIN
  stub.cpp)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
  # Force all symbols from static archives into the dylib.
  set(JIT_LIB_NAMES -Wl,-all_load ${JIT_LIB_NAMES})
else()
  set(JIT_LIB_NAMES -Wl,--whole-archive ${JIT_LIB_NAMES} -Wl,--no-whole-archive)
endif()

target_link_libraries(KNHR PRIVATE ${JIT_LIB_NAMES})

if(APPLE)
  set_property(TARGET KNHR APPEND_STRING PROPERTY
    LINK_FLAGS
    " -compatibility_version 1 -current_version ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}.${LLVM_VERSION_PATCH}")
endif()
