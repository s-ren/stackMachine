"builtin.module"() ({
  "stackIR.load"() <{index = 3 : i8, stack_ptr = 0 : i8}> : () -> ()
  "stackIR.load"() <{index = 7 : i8, stack_ptr = 1 : i8}> : () -> ()
  "stackIR.pop"() <{stack_ptr = 2 : i8}> : () -> ()
  "stackIR.load"() <{index = 7 : i8, stack_ptr = 1 : i8}> : () -> ()
  "stackIR.sub"() <{stack_ptr = 2 : i8}> : () -> ()
  "stackIR.store"() <{index = 5 : i8, stack_ptr = 1 : i8}> : () -> ()
  "stackIR.dup"() <{stack_ptr = 1 : i8}> : () -> ()
  "stackIR.stop"() <{stack_ptr = 2 : i8}> : () -> ()
}) {stackIR.stack_size = 2 : i32} : () -> ()

