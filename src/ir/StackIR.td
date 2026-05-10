include "mlir/IR/OpBase.td"
include "mlir/IR/BuiltinTypes.td"
include "mlir/IR/AttrTypeBase.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

// define the stack dialect
def StackIR_Dialect : Dialect {
  let name = "stackIR";
  let summary = "IR for a simple stack machine.";
  let cppNamespace = "::mlir::stackIR";
}

// define a base class for all stack operations
class StackIR_Op<string mnemonic, list<Trait> traits = []> :
   Op<StackIR_Dialect, mnemonic, traits> {
    let arguments = (ins I32Attr:$stack_ptr);
   }

def StopOp: StackIR_Op<"stop", []> {
  let summary = "stop code. Ends execution.";
  let description = [{
    This operation represents the stopping point of the program.
    Further instructions after this operation will not be executed and the compiler optimizes them away.
  }];
}

def LoadOp: StackIR_Op<"load", []> {
  let summary = "load code. Pushes a value onto the stack.";
  let description = [{
    This operation represents pushing the ith word from the input file onto the stack.
  }];
  let arguments = (ins I8Attr:$index, I32Attr:$stack_ptr);
}

def StoreOp: StackIR_Op<"store", []> {
  let summary = "store code. Stores the top value from the stack.";
  let description = [{
    This operation represents takingthe top value from the stack and writing it to the ith word in the output file.
  }];
  let arguments = (ins I8Attr:$index, I32Attr:$stack_ptr);
}

def PopOp: StackIR_Op<"pop", []> {
  let summary = "pop code. Removes the top value from the stack.";
  let description = [{
    This operation represents removing the top value from the stack.
  }];
}

def AddOp: StackIR_Op<"add", []> {
  let summary = "add code. Adds the top two values on the stack.";
  let description = [{
    This operation represents taking the top two values from the stack, adding them together, and pushing the result back onto the stack.
  }];
}

def SubOp: StackIR_Op<"sub", []> {
  let summary = "sub code. Subtracts the top two values on the stack.";
  let description = [{
    This operation represents taking the top two values from the stack, subtracting the second value from the first, and pushing the result back onto the stack.
  }];
}

def DupOp: StackIR_Op<"dup", []> {
  let summary = "dup code. Duplicates the top value on the stack.";
  let description = [{
    This operation represents taking the top value from the stack and pushing a copy of it back onto the stack.
  }];
}