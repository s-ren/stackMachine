import random
import argparse

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

def generate_random_bytecode(num_instructions, output_file, max_index=255):
    """
    Generates a structurally valid sequence of stack machine bytecode.
    """
    opcodes_no_args = [POP, ADD, SUB, DUP]
    opcodes_with_args = [LOAD, STORE]

    bytecode = bytearray()
    listing = []  # list of (opcode, arg_or_None) tuples

    # Generate instructions (reserving the last one for a guaranteed STOP)
    for _ in range(num_instructions - 1):

        # 70% chance to do math/stack ops, 30% chance to do memory ops
        if random.random() < 0.70:
            op = random.choice(opcodes_no_args)
            bytecode.append(op)
            listing.append((op, None))
        else:
            op = random.choice(opcodes_with_args)
            operand = random.randint(0, max_index)
            bytecode.append(op)
            bytecode.append(operand)
            listing.append((op, operand))

    # Guarantee the program terminates cleanly
    bytecode.append(STOP)
    listing.append((STOP, None))

    # Write the raw bytes to the binary file
    with open(output_file, 'wb') as f:
        f.write(bytecode)

    # Write the human-readable listing alongside the binary
    listing_file = output_file + ".txt"
    with open(listing_file, 'w') as f:
        for op, arg in listing:
            name = OPCODE_NAMES.get(op, f"0x{op:02x}")
            line = f"{name} {arg}" if arg is not None else name
            f.write(line + "\n")

    print(f"[*] Generated {num_instructions} instructions ({len(bytecode)} bytes).")
    print(f"[*] Saved to -> {output_file}")
    print(f"[*] Listing  -> {listing_file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="StackIR Bytecode Fuzzer")
    parser.add_argument("-n", "--num", type=int, default=100, help="Number of instructions to generate")
    parser.add_argument("-o", "--out", type=str, default="test.bin", help="Output binary file name")
    parser.add_argument("-m", "--max-index", type=int, default=255,
                        help="Maximum index for LOAD/STORE to constrain memory bounds")

    args = parser.parse_args()
    generate_random_bytecode(args.num, args.out, args.max_index)
