"builtin.module"() ({
  "stackIR.load"() <{index = 6 : i8, stack_ptr = 0 : i8}> : () -> ()
  "stackIR.load"() <{index = 6 : i8, stack_ptr = 1 : i8}> : () -> ()
  "stackIR.sub"() <{stack_ptr = 2 : i8}> : () -> ()
  "stackIR.load"() <{index = 0 : i8, stack_ptr = 1 : i8}> : () -> ()
  "stackIR.store"() <{index = 6 : i8, stack_ptr = 2 : i8}> : () -> ()
}) {stackIR.stack_size = 2 : i32} : () -> ()

