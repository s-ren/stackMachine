find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(STACK_FUZZ_RUNNER ${CMAKE_CURRENT_SOURCE_DIR}/fuzzer.py)

add_test(
  NAME stack-fuzz
  COMMAND ${Python3_EXECUTABLE} ${STACK_FUZZ_RUNNER}
)