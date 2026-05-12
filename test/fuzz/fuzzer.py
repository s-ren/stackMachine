import random

# --- Opcode Definitions ---
STOP  = 0x00
LOAD  = 0x01
STORE = 0x02
POP   = 0x03
ADD   = 0x04
SUB   = 0x05
DUP   = 0x06

OPCODE_NAMES = {
    STOP:  "stop",
    LOAD:  "load",
    STORE: "store",
    POP:   "pop",
    ADD:   "add",
    SUB:   "sub",
    DUP:   "dup",
}


def interpret_bytecode(bytecode):
    """
    Interprets raw stack-machine bytecode and returns the final machine state.

    - bytecode: bytes-like object containing opcodes and operands
    - in_words: optional list of input words; defaults to 256 zero words
    - out_words_size: number of output words to model; defaults to 64
    """
    out_words_size = 64
    in_words = [0] * 256
    out_words = [0] * out_words_size
    code = bytes(bytecode)
    stack = []
    pc = 0
    executed_ops = []
    stopped = False

    while pc < len(code):
        opcode = code[pc]
        operand = None

        if opcode == STOP:
            executed_ops.append((opcode, operand))
            stopped = True
            break

        if opcode in (LOAD, STORE):
            if pc + 1 >= len(code):
                raise ValueError(f"Missing operand for {OPCODE_NAMES[opcode]} at byte offset {pc}")
            operand = code[pc + 1]

        if opcode == LOAD:
            if operand >= len(in_words):
                raise IndexError(f"LOAD index {operand} out of bounds for input of size {len(in_words)}")
            stack.append(in_words[operand])
            pc += 2
        elif opcode == STORE:
            if not stack:
                raise ValueError(f"Stack underflow for STORE at byte offset {pc}")
            if operand >= out_words_size:
                raise IndexError(f"STORE index {operand} out of bounds for output of size {out_words_size}")
            out_words[operand] = stack[-1]
            pc += 2
        elif opcode == POP:
            if not stack:
                raise ValueError(f"Stack underflow for POP at byte offset {pc}")
            stack.pop()
            pc += 1
        elif opcode == ADD:
            if len(stack) < 2:
                raise ValueError(f"Stack underflow for ADD at byte offset {pc}")
            rhs = stack.pop()
            lhs = stack.pop()
            stack.append(lhs + rhs)
            pc += 1
        elif opcode == SUB:
            if len(stack) < 2:
                raise ValueError(f"Stack underflow for SUB at byte offset {pc}")
            top = stack.pop()
            below_top = stack.pop()
            stack.append(top - below_top)
            pc += 1
        elif opcode == DUP:
            if not stack:
                raise ValueError(f"Stack underflow for DUP at byte offset {pc}")
            stack.append(stack[-1])
            pc += 1
        else:
            raise ValueError(f"Unknown opcode 0x{opcode:02x} at byte offset {pc}")

        executed_ops.append((opcode, operand))

    return {
        "stack": stack,
        "out_words": out_words,
        "stopped": stopped,
        "pc": pc,
        "executed_ops": executed_ops,
    }


def generate_random_bytecode(num_instructions = 100, max_index=255):
    """
    Generates a structurally valid sequence of stack machine bytecode.
    """
    opcodes_no_args = [POP, ADD, SUB, DUP]
    opcodes_with_args = [LOAD, STORE]

    bytecode = bytearray()
    listing = []  # list of (opcode, arg_or_None) tuples

    # Generate instructions (reserving the last one for a guaranteed STOP)
    for _ in range(num_instructions - 1):

        # 69% chance to do math/stack ops, 30% chance to do memory ops, 1% STOP
        if random.random() < 0.69:
            op = random.choice(opcodes_no_args)
            bytecode.append(op)
            listing.append((op, None))
        elif random.random() < 0.7:
            bytecode.append(STOP)
            listing.append((STOP, None))
        else:
            op = random.choice(opcodes_with_args)
            operand = random.randint(0, max_index)
            bytecode.append(op)
            bytecode.append(operand)
            listing.append((op, operand))

if __name__ == "__main__":
    test_cases = generate_random_bytecode(100, 255)
    for test in test_cases:
        # write the test case to a file and run bin/stack on it with input 
        try:
            result = interpret_bytecode(test)
        except Exception as e:

