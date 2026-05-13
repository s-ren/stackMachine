import random
import shutil
import subprocess
import sys
from pathlib import Path

# --- Opcode Definitions ---
STOP = 0x00
LOAD = 0x01
STORE = 0x02
POP = 0x03
ADD = 0x04
SUB = 0x05
DUP = 0x06

WORD_BYTES = 32
INPUT_WORDS = 256
OUTPUT_WORDS = 64
WORD_MODULUS = 1 << (WORD_BYTES * 8)

OPCODE_NAMES = {
    STOP: "stop",
    LOAD: "load",
    STORE: "store",
    POP: "pop",
    ADD: "add",
    SUB: "sub",
    DUP: "dup",
}

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent
STACKC_PATH = REPO_ROOT / "bin" / "stackc"
CORPUS_DIR = SCRIPT_DIR / "corpus"
NUM_TEST_CASES = 300
NUM_INSTRUCTIONS = 300
RANDOM_SEED = 0

def mask_word(value: int) -> int:
    return value % WORD_MODULUS


def interpret_bytecode(bytecode, in_words=None) -> list:
    out_words = [0] * OUTPUT_WORDS
    words = list(range(INPUT_WORDS)) if in_words is None else in_words
    code = bytes(bytecode)
    stack = []
    pc = 0

    while pc < len(code):
        opcode = code[pc]
        operand = None

        if opcode == STOP:
            return out_words

        if opcode in (LOAD, STORE):
            if pc + 1 >= len(code):
                raise RuntimeError(f"Missing operand for {OPCODE_NAMES[opcode]} at byte offset {pc}")
            operand = code[pc + 1]

        if opcode == LOAD:
            if operand >= len(words):
                raise RuntimeError(f"LOAD index {operand} out of bounds for input of size {len(words)}")
            stack.append(mask_word(words[operand]))
            pc += 2
        elif opcode == STORE:
            if operand >= OUTPUT_WORDS:
                raise RuntimeError(f"STORE index {operand} out of bounds for output of size {OUTPUT_WORDS}")
            if not stack:
                raise RuntimeError(f"Stack underflow for STORE at byte offset {pc}")
            out_words[operand] = stack[-1]
            pc += 2
        elif opcode == POP:
            if not stack:
                raise RuntimeError(f"Stack underflow for POP at byte offset {pc}")
            stack.pop()
            pc += 1
        elif opcode == ADD:
            if len(stack) < 2:
                raise RuntimeError(f"Stack underflow for ADD at byte offset {pc}")
            rhs = stack.pop()
            lhs = stack.pop()
            stack.append(mask_word(lhs + rhs))
            pc += 1
        elif opcode == SUB:
            if len(stack) < 2:
                raise RuntimeError(f"Stack underflow for SUB at byte offset {pc}")
            top = stack.pop()
            below_top = stack.pop()
            stack.append(mask_word(top - below_top))
            pc += 1
        elif opcode == DUP:
            if not stack:
                raise RuntimeError(f"Stack underflow for DUP at byte offset {pc}")
            stack.append(stack[-1])
            pc += 1
        else:
            raise RuntimeError(f"Unknown opcode 0x{opcode:02x} at byte offset {pc}")

    raise RuntimeError("No STOP opcode found in bytecode")


def generate_random_bytecode(num_instructions=100, max_index=255):
    weighted_ops = [STOP, LOAD, STORE, POP, ADD, SUB, DUP]
    weights = [1, 50, 11, 5, 11, 11, 11]
    bytecode = bytearray()
    saw_stop = False

    for _ in range(num_instructions):
        opcode = random.choices(weighted_ops, weights=weights, k=1)[0]
        bytecode.append(opcode)
        if opcode in (LOAD, STORE):
            bytecode.append(random.randint(0, max_index))
        if opcode == STOP:
            saw_stop = True
            if random.random() < 0.75:
                break

    if not saw_stop and random.random() < 0.8:
        bytecode.append(STOP)

    return bytes(bytecode)


def write_input_file(path: Path, words) -> None:
    payload = bytearray()
    for value in words:
        payload.extend(mask_word(value).to_bytes(WORD_BYTES, byteorder="big", signed=False))
    path.write_bytes(payload)


def decode_words(path: Path, count: int) -> list:
    data = path.read_bytes()
    expected_size = count * WORD_BYTES
    if len(data) != expected_size:
        raise RuntimeError(f"Expected {expected_size} bytes in {path}, got {len(data)}")

    decoded = []
    for index in range(0, len(data), WORD_BYTES):
        decoded.append(int.from_bytes(data[index:index + WORD_BYTES], byteorder="big", signed=False))
    return decoded


def write_text_file(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def format_bytecode(bytecode: bytes) -> str:
    return " ".join(f"0x{byte:02x}" for byte in bytecode)


def format_words(words) -> str:
    return "\n".join(str(word) for word in words) + "\n"


def prepare_corpus_dir(corpus_dir: Path) -> None:
    if corpus_dir.exists():
        shutil.rmtree(corpus_dir)
    corpus_dir.mkdir(parents=True, exist_ok=True)


def evaluate_case(case_dir: Path, bytecode: bytes, in_words) -> tuple[bool, str]:
    # prepare file path
    program_path = case_dir / "program.bin"
    input_path = case_dir / "in.bin"
    output_path = case_dir / "out"
    output_base = case_dir / "program"
    executable_path = output_base.with_suffix(".out")

    # write program to file
    program_path.write_bytes(bytecode)
    write_text_file(case_dir / "program.txt", format_bytecode(bytecode) + "\n")
    write_input_file(input_path, in_words)

    try:
        # interpret the bytecode
        expected_out = interpret_bytecode(bytecode, in_words)
        interpreter_error = None
        write_text_file(case_dir / "interpreter.txt", format_words(expected_out))
    except RuntimeError as exc:
        expected_out = None
        interpreter_error = str(exc)
        write_text_file(case_dir / "interpreter-error.txt", interpreter_error + "\n")

    # compile stackc 
    compile_proc = subprocess.run(
        [str(STACKC_PATH), "-i", str(program_path), "-o", str(output_base)],
        capture_output=True,
        text=True,
        check=False,
    )
    write_text_file(case_dir / "stackc.stdout.txt", compile_proc.stdout)
    write_text_file(case_dir / "stackc.stderr.txt", compile_proc.stderr)

    # catch compiler error and report mismatch
    if compile_proc.returncode != 0:
        if interpreter_error is not None:
            return True, "accepted: stackc compile error and interpreter runtime error"
        return False, "rejected: stackc failed but interpreter succeeded"

    if interpreter_error is not None:
        return False, "rejected: stackc compiled but interpreter failed"

    # run compiled binary
    run_proc = subprocess.run(
        [str(executable_path), str(input_path), str(output_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    write_text_file(case_dir / "exec.stdout.txt", run_proc.stdout)
    write_text_file(case_dir / "exec.stderr.txt", run_proc.stderr)

    # runtime error! report mismatch.
    if run_proc.returncode != 0:
        return False, f"rejected: executable exited with {run_proc.returncode}"

    # compare diff between interpreter and executable output
    actual_out = decode_words(output_path, OUTPUT_WORDS)
    write_text_file(case_dir / "out.decoded.txt", format_words(actual_out))

    if actual_out != expected_out:
        return False, "rejected: executable output mismatch"

    return True, "accepted: executable output matched interpreter"


if __name__ == "__main__":
    print(f"Running fuzzer with {NUM_TEST_CASES} test cases, each with up to {NUM_INSTRUCTIONS} instructions.")
    # check stackc exists before doing any work
    if not STACKC_PATH.is_file():
        print(f"error: stackc not found at {STACKC_PATH}", file=sys.stderr)
        raise SystemExit(2)

    # set up testing directories
    #random.seed(RANDOM_SEED)
    prepare_corpus_dir(CORPUS_DIR)

    summaries = []
    all_passed = True

    for case_index in range(NUM_TEST_CASES):
        if case_index % 50 == 0:
            print(f"Running test case {case_index}")
        case_dir = CORPUS_DIR / f"case-{case_index:04d}"
        case_dir.mkdir(parents=True, exist_ok=True)
        # generate bytecode and input 
        test_case = generate_random_bytecode(NUM_INSTRUCTIONS, 255)
        input_words = [random.getrandbits(WORD_BYTES * 8) for _ in range(INPUT_WORDS)]
        # evaluate
        accepted, reason = evaluate_case(case_dir, test_case, input_words)
        #status = "PASS" if accepted else "FAIL"
        if not accepted:
            summary = f"FAIL: case-{case_index:04d}: {reason}\n{test_case.hex()}\n" 
            print(summary)
            summaries.append(summary)
        all_passed = all_passed and accepted
    # write to summary
    write_text_file(SCRIPT_DIR / "summary.txt", "\n".join(summaries) + "\n")
    if all_passed:
        print(f"All {NUM_TEST_CASES} test cases passed!")
    raise SystemExit(0 if all_passed else 1)

