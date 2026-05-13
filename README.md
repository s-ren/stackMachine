# Stack machine design

A simple compiler from stack machine to x86.

High level compilation process: bytecode -> StackIR -> LLVM -> X86.

## Stack Machine
Code:
```
00: STOP; 01 i: LOAD i; 02 i: STORE i; 03: POP; 04: ADD; 05: SUB; 06: DUP
```

## Stack IR
```
instruction = STOP | LOAD i | STORE i | POP | ADD | SUB | DUP
op = (instruction, p)
STACK_SIZE
```

### Static analysis
All control flow in this language are static, so stack underbound errors can be detected at compile time.

Our static pass decorates each node with the stack height ``p``.

``STACK_SIZE`` is set to the maximum of ``p`` after the IR pass,
and it is used as a metavariable for LLVM lowering.

### Lowering to LLVM
```
Initialization:
%stack = alloca [STACK_SIZE x i256]
%in is input pointer
%out is output pointer

STOP, p -> 
ret void;

LOAD i, p ->
// load from input array index i
%in_ptr = getelementptr i256, ptr %in i8 i;
%val = load i256, ptr %in_ptr;

// store to stack array index p
%stack_ptr = getelementptr i256, ptr %stack i8 p;
store i256 %val, ptr %stack_ptr


STORE i, p ->
// load from stack array index (p-1)
%stack_ptr = getelementptr i256, ptr %stack i8 (p-1);
%val = load i256, ptr %stack_ptr;

// store to output array index i
%out_ptr = getelementptr i256, ptr %out i8 i;
store i256 %val, ptr %out_ptr

POP, p -> 
// noop

ADD, p ->
// load from stack array index p-2
%stack_ptr_1 = getelementptr i256, ptr %stack i8 (p-2);
%val1 = load i256, ptr %stack_ptr_1;

// load from stack array index p-1
%stack_ptr_2 = getelementptr i256, ptr %stack i8 (p-1);
%val2 = load i256, ptr %stack_ptr_2;

// compute result
%val = add i256 %val1, %val2;

// store to stack array index p-2
%stack_ptr = getelementptr i256, ptr %stack i8 (p-2);
store i256 %val, ptr %stack_ptr

SUB, p ->
// load from stack array index p-1
%stack_ptr_1 = getelementptr i256, ptr %stack i8 (p-1);
%val1 = load i256, ptr %stack_ptr_1;

// load from stack array index p-2
%stack_ptr_2 = getelementptr i256, ptr %stack i8 (p-2);
%val2 = load i256, ptr %stack_ptr_2;

// compute result
%val = sub i256 %val1, %val2;

// store to stack array index p-2
%stack_ptr = getelementptr i256, ptr %stack i8 (p-2);
store i256 %val, ptr %stack_ptr

DUP, p ->
// load from stack array index p-1
%stack_ptr = getelementptr i256, ptr %stack i8 (p-1);
%val = load i256, ptr %stack_ptr;

// store to stack array index p
%stack_ptr = getelementptr i256, ptr %stack i8 p;
store i256 %val, ptr %stack_ptr
```
``stack`` points to the start of the stack;

``in`` points to the start of input file;

``out`` points to the start of output file.

### Enforcing big endian 
To ensure big endian format, the wrapper preprocess and post process the input and output.
It turns the input into machine endian before processing.

# Build and run
To build:
```
bash build.sh
```
To run:
```
./bin/stackc --help
```