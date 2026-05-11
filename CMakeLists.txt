cmake_minimum_required(VERSION 3.20)
project(stack CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Find MLIR
find_package(MLIR REQUIRED CONFIG
  HINTS /usr/lib/llvm-18/lib/cmake/mlir)

message(STATUS "Using MLIR in: ${MLIR_DIR}")

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

include(TableGen)
include(AddLLVM)
include(AddMLIR)

# Locate mlir-tblgen
find_program(MLIR_TBLGEN_EXE mlir-tblgen
  HINTS ${LLVM_TOOLS_BINARY_DIR}
  REQUIRED)

# ── TableGen ────────────────────────────────────────────────────────────────
# Generated .inc files are written directly into src/ir/ alongside StackIR.td.

set(TD_FILE  ${CMAKE_SOURCE_DIR}/src/ir/StackIR.td)
set(INC_DIR  ${CMAKE_SOURCE_DIR}/src/ir)

# Build the -I flag list for mlir-tblgen
set(TBLGEN_INCLUDES -I ${CMAKE_SOURCE_DIR}/src/ir)
foreach(_inc ${MLIR_INCLUDE_DIRS})
  list(APPEND TBLGEN_INCLUDES -I ${_inc})
endforeach()

function(tablegen_output output_file)
  set(flags ${ARGN})
  add_custom_command(
    OUTPUT  ${INC_DIR}/${output_file}
    COMMAND ${MLIR_TBLGEN_EXE} ${flags} ${TBLGEN_INCLUDES}
            ${TD_FILE} --write-if-changed -o ${INC_DIR}/${output_file}
    DEPENDS ${TD_FILE}
    COMMENT "TableGen: ${output_file}"
    VERBATIM
  )
endfunction()

tablegen_output(StackIROps.h.inc      -gen-op-decls)
tablegen_output(StackIROps.cpp.inc    -gen-op-defs)
tablegen_output(StackIRDialect.h.inc  -gen-dialect-decls -dialect=stackIR)
tablegen_output(StackIRDialect.cpp.inc -gen-dialect-defs -dialect=stackIR)

add_custom_target(StackIRIncGen DEPENDS
  ${INC_DIR}/StackIROps.h.inc
  ${INC_DIR}/StackIROps.cpp.inc
  ${INC_DIR}/StackIRDialect.h.inc
  ${INC_DIR}/StackIRDialect.cpp.inc
)

# ── Executable ──────────────────────────────────────────────────────────────

file(GLOB_RECURSE STACK_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(stack ${STACK_SOURCES})
add_dependencies(stack StackIRIncGen)

target_compile_definitions(stack PRIVATE
  STACK_HOST_CXX="${CMAKE_CXX_COMPILER}"
  STACK_WRAPPER_SOURCE="${CMAKE_SOURCE_DIR}/wrapper/wrapper.cpp"
)

target_include_directories(stack PRIVATE
  ${MLIR_INCLUDE_DIRS}
  ${LLVM_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/ir
  ${CMAKE_SOURCE_DIR}/src/include
)

target_link_libraries(stack PRIVATE
  MLIRIR
  MLIRParser
  MLIRSupport
  MLIRLLVMDialect
  MLIRTargetLLVMIRExport
  MLIRLLVMToLLVMIRTranslation
  MLIRBuiltinToLLVMIRTranslation
)
