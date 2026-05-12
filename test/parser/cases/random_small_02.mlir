"builtin.module"() ({
  "stackIR.load"() <{index = 1 : i8, stack_ptr = 0 : i8}> : () -> ()
  "stackIR.pop"() <{stack_ptr = 1 : i8}> : () -> ()
  "stackIR.load"() <{index = 3 : i8, stack_ptr = 0 : i8}> : () -> ()
  "stackIR.pop"() <{stack_ptr = 1 : i8}> : () -> ()
  "stackIR.load"() <{index = 6 : i8, stack_ptr = 0 : i8}> : () -> ()
}) {stackIR.stack_size = 1 : i32} : () -> ()

