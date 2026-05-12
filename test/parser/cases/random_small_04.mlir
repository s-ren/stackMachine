"builtin.module"() ({
  "stackIR.load"() <{index = 4 : i8, stack_ptr = 0 : i8}> : () -> ()
  "stackIR.load"() <{index = 4 : i8, stack_ptr = 1 : i8}> : () -> ()
  "stackIR.store"() <{index = 7 : i8, stack_ptr = 2 : i8}> : () -> ()
  "stackIR.load"() <{index = 1 : i8, stack_ptr = 2 : i8}> : () -> ()
  "stackIR.store"() <{index = 4 : i8, stack_ptr = 3 : i8}> : () -> ()
}) {stackIR.stack_size = 3 : i32} : () -> ()

