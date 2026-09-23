single_fault_conf = configuration_data()
single_fault_conf.set('TMRX_SO', plugin_path)

configure_file(
  input: 'single_fault.sby.in',
  output: 'single_fault.sby',
  configuration: single_fault_conf,
)

configure_file(
  input: '../../fault_injection_tests/fi_counter/fi_counter.v',
  output: 'fi_counter.v',
  copy: true,
)
configure_file(
  input: '../../fault_injection_tests/fi_counter/tmrx_config.toml',
  output: 'tmrx_config.toml',
  copy: true,
)
configure_file(input: 'formal_top.sv', output: 'formal_top.sv', copy: true)

test(
  'fi_counter_single_fault_proof',
  sby,
  args: ['-f', 'single_fault.sby'],
  timeout: 3600,
  workdir: meson.current_build_dir(),
  depends: tmrx,
  suite: 'formal_equiv_tests',
)
