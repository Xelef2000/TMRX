sby = find_program('sby', required: false)

if not sby.found()
  warning('SymbiYosys (sby) not found – skipping formal_equiv_tests suite')
else
  subdir('simple_ff')
  subdir('counter_8bit')
  subdir('single_fault')
  subdir('eval_designs')
  subdir('config_complex')
  subdir('sub_module_tests')
  subdir('croc')
endif
