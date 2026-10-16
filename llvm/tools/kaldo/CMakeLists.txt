# Builds libKaldo, a shared library containing
# the LLVM ORC JIT and JITLink components needed by Kotlin/Native hot-reload.
#
# Building for iOS Simulator as target
# Enable with: -DLLVM_BUILD_KOTLIN_NATIVE_KALDO=ON
#   cmake -G Ninja \
#       -DCMAKE_BUILD_TYPE=Release \
#       -DCMAKE_SYSTEM_NAME=iOS \
#       -DCMAKE_OSX_SYSROOT=iphonesimulator \
#       -DCMAKE_OSX_ARCHITECTURES=arm64 \
#       -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
#       -DCMAKE_MACOSX_BUNDLE=OFF \
#       -DLLVM_ENABLE_LIBCXX=ON \
#       -DLLVM_ENABLE_RTTI=ON \
#       -DLLVM_BUILD_KOTLIN_NATIVE_KALDO=ON \
#       -DLLVM_TARGETS_TO_BUILD="AArch64" \
#       -DLLVM_ENABLE_PROJECTS="llvm" \
#       -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \

option(LLVM_BUILD_KOTLIN_NATIVE_KALDO "Build libKaldo shared library for Kotlin/Native Hot-Reload" OFF)

if (NOT LLVM_BUILD_KOTLIN_NATIVE_KALDO)
    return()
endif ()

set(KALDO_COMPONENTS
    OrcJIT
    OrcDebugging
    OrcShared
    OrcTargetProcess
    JITLink
    ExecutionEngine
    RuntimeDyld
)

# Conditionally add target backends based on what LLVM was configured with.
# AArch64 covers macOS ARM64, iOS device, iOS Simulator on Apple Silicon.
# X86 covers macOS x86_64, iOS Simulator on Intel.
foreach (target IN ITEMS AArch64 X86)
    if ("${target}" IN_LIST LLVM_TARGETS_TO_BUILD)
        list(APPEND KALDO_COMPONENTS
                ${target}CodeGen
                ${target}AsmParser
                ${target}Desc
                ${target}Disassembler
                ${target}Info
        )
        if ("${target}" STREQUAL "AArch64")
            list(APPEND KALDO_COMPONENTS AArch64Utils)
        endif ()
    endif ()
endforeach ()

llvm_map_components_to_libnames(JIT_LIB_NAMES ${KALDO_COMPONENTS})
list(REMOVE_DUPLICATES JIT_LIB_NAMES)

string(TOLOWER "${CMAKE_OSX_SYSROOT}" SYSROOT_LOWER)

if (SYSROOT_LOWER MATCHES "iphonesimulator")
    set(KALDO_OUTPUT_NAME "Kaldo.iossim")
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(KALDO_OUTPUT_NAME "Kaldo.ios")
else()
    set(KALDO_OUTPUT_NAME "Kaldo")
endif()

if(APPLE)
    # For some reasons, -z is passed to the Xcode linker which is not supported by
    string(REPLACE "-Wl,-z,defs" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
endif()

message(STATUS "Kaldo output name set to: ${KALDO_OUTPUT_NAME}, sysroot: ${CMAKE_OSX_SYSROOT}")

add_llvm_library(
        Kaldo SHARED
        DISABLE_LLVM_LINK_LLVM_DYLIB
        OUTPUT_NAME ${KALDO_OUTPUT_NAME}
        INSTALL_WITH_TOOLCHAIN

        Expected.hpp
        KaldoInternal.hpp
        Kaldo.hpp
        ObjCInteropManager.hpp
        ObjCInterop.hpp
        ReloadUnitHandle.hpp
        Utility.hpp

        Kaldo.cpp
        KaldoInternal.cpp
        ObjCInteropManager.cpp
        ManifestReader.cpp

        plugins/KotlinSymbolExternalizer.hpp
        plugins/KotlinSymbolExternalizer.cpp
        plugins/MachOHostDataSymbolGenerator.hpp
        plugins/MachOHostDataSymbolGenerator.cpp
        plugins/ObjCSelectorFixupPlugin.hpp
        plugins/ObjCSelectorFixupPlugin.cpp
        plugins/WeakSymbolFallbackGenerator.hpp
        plugins/WeakSymbolFallbackGenerator.cpp
)

target_link_libraries(Kaldo PRIVATE ${JIT_LIB_NAMES})

target_include_directories(Kaldo
        PUBLIC
        $<BUILD_INTERFACE:${LLVM_MAIN_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/Expected.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Kaldo.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ObjCInterop.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ReloadUnitHandle.hpp
        DESTINATION "include/llvm/Kaldo"
        COMPONENT Kaldo)

if (APPLE)
    target_link_libraries(Kaldo PRIVATE "-framework Foundation" objc)

    target_link_options(Kaldo PRIVATE -Wl,-dead_strip)

    # The symbols `_KNHR_LoadObjCStubAddress` ... needs to be resolved from the Kotlin/Native runtime.
    # Except, for `llvm_orc` ones, since `-dead_strip` removes them from the final Kaldo aritfact,
    # making debug symbols gone for GDBJITDebugInfoRegistrationPlugin.
    target_link_options(Kaldo
            PRIVATE
            -Wl,-U,_KNHR_LoadObjCStubAddress
            -Wl,-U,_Kotlin_ObjCExport_initializeTypeAdaptersWithPointers
            -Wl,-U,_Kotlin_ObjCInterop_uniquePrefix
            -Wl,-u,_llvm_orc_registerJITLoaderGDBAllocAction
    )

    set_property(TARGET Kaldo APPEND_STRING PROPERTY
            LINK_FLAGS
            " -compatibility_version 1 -current_version ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}.${LLVM_VERSION_PATCH}")
endif ()
