cmake_minimum_required(VERSION 3.20)
project(stack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Find MLIR.
set(STACK_MLIR_CMAKE_DIR "" CACHE PATH "Path to the directory containing MLIRConfig.cmake")
set(STACK_LLVM_CMAKE_DIR "" CACHE PATH "Path to the directory containing LLVMConfig.cmake")

set(_stack_mlir_hints)
foreach(_mlir_hint IN ITEMS "${STACK_MLIR_CMAKE_DIR}" "$ENV{MLIR_DIR}")
  if(_mlir_hint)
    list(APPEND _stack_mlir_hints "${_mlir_hint}")
  endif()
endforeach()

foreach(_llvm_hint IN ITEMS "${STACK_LLVM_CMAKE_DIR}" "$ENV{LLVM_DIR}")
  if(NOT _llvm_hint)
    continue()
  endif()

  list(APPEND _stack_mlir_hints "${_llvm_hint}")
  get_filename_component(_stack_cmake_parent "${_llvm_hint}" DIRECTORY)
  if(EXISTS "${_stack_cmake_parent}/mlir")
    list(APPEND _stack_mlir_hints "${_stack_cmake_parent}/mlir")
  endif()
endforeach()

find_program(STACK_LLVM_CONFIG_EXE NAMES llvm-config)
if(STACK_LLVM_CONFIG_EXE)
  execute_process(
    COMMAND ${STACK_LLVM_CONFIG_EXE} --cmakedir
    OUTPUT_VARIABLE _stack_llvm_config_cmakedir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _stack_llvm_config_result
    ERROR_QUIET
  )

  if(_stack_llvm_config_result EQUAL 0 AND _stack_llvm_config_cmakedir)
    list(APPEND _stack_mlir_hints "${_stack_llvm_config_cmakedir}")
    get_filename_component(_stack_cmake_parent "${_stack_llvm_config_cmakedir}" DIRECTORY)
    if(EXISTS "${_stack_cmake_parent}/mlir")
      list(APPEND _stack_mlir_hints "${_stack_cmake_parent}/mlir")
    endif()
  endif()
endif()

file(GLOB _stack_common_mlir_hints LIST_DIRECTORIES true
  "/usr/lib/llvm*/lib/cmake/mlir"
  "/usr/local/lib/llvm*/lib/cmake/mlir"
  "/opt/homebrew/opt/llvm*/lib/cmake/mlir"
  "/opt/local/libexec/llvm*/lib/cmake/mlir"
)
list(APPEND _stack_mlir_hints ${_stack_common_mlir_hints})

list(REMOVE_DUPLICATES _stack_mlir_hints)

find_package(MLIR REQUIRED CONFIG HINTS ${_stack_mlir_hints})

message(STATUS "Using MLIR in: ${MLIR_DIR}")
message(STATUS "Using LLVM in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

include(TableGen)
include(AddLLVM)
include(AddMLIR)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(STACK_FUZZ_RUNNER ${CMAKE_SOURCE_DIR}/test/fuzz/fuzzer.py)

# Locate mlir-tblgen
find_program(MLIR_TBLGEN_EXE mlir-tblgen
  HINTS ${LLVM_TOOLS_BINARY_DIR}
  REQUIRED)

# ── TableGen ────────────────────────────────────────────────────────────────
# Generated .inc files live in the build tree so out-of-source builds do not
# mutate the checkout and multiple build directories can coexist.

set(TD_FILE  ${CMAKE_SOURCE_DIR}/src/ir/StackIR.td)
set(STACK_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(INC_DIR  ${STACK_GENERATED_INCLUDE_DIR}/ir)

file(MAKE_DIRECTORY ${INC_DIR})

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
    DEPENDS ${TD_FILE} ${MLIR_TBLGEN_EXE}
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
set_target_properties(stack PROPERTIES OUTPUT_NAME stackc)

target_compile_definitions(stack PRIVATE
  STACK_HOST_CXX="${CMAKE_CXX_COMPILER}"
  STACK_WRAPPER_SOURCE="${CMAKE_SOURCE_DIR}/wrapper/wrapper.cpp"
)

target_include_directories(stack PRIVATE
  ${MLIR_INCLUDE_DIRS}
  ${LLVM_INCLUDE_DIRS}
  ${STACK_GENERATED_INCLUDE_DIR}
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

enable_testing()
add_subdirectory(test)
