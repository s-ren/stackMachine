add_executable(stack-parser-driver
  driver.cpp
  ${CMAKE_SOURCE_DIR}/src/parser/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ir/StackIRDialect.cpp
)

set_target_properties(stack-parser-driver PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(STACK_PARSER_RUNNER ${CMAKE_CURRENT_SOURCE_DIR}/run_cases.py)

add_dependencies(stack-parser-driver StackIRIncGen)

target_include_directories(stack-parser-driver PRIVATE
  ${MLIR_INCLUDE_DIRS}
  ${LLVM_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/ir
  ${CMAKE_SOURCE_DIR}/src/include
)

target_link_libraries(stack-parser-driver PRIVATE
  MLIRIR
  MLIRParser
  MLIRSupport
)

add_test(
  NAME stack-parser-cases
  COMMAND ${Python3_EXECUTABLE} ${STACK_PARSER_RUNNER}
)

add_custom_target(check-stack-parser ALL
  COMMAND ${Python3_EXECUTABLE} ${STACK_PARSER_RUNNER}
  DEPENDS stack-parser-driver
  USES_TERMINAL
)