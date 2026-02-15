test1_conf = configuration_data()
test1_conf.set('PDK_ROOT', pdk_abs_path)

configure_file(
  input: 'config_test.ys.in',
  output: 'config_test.ys',
  configuration: test1_conf,
)

configure_file(input: './top.v', output: 'top.v', copy: true)

test(
  'config_test_global_1',
  yosys,
  args: [
    '-ql', 'ihp_synth_flattened.log',
    '-m', plugin_path,
    '-s', 'ihp_synth_flattened.ys',
    '-v', '10',
  ],
  timeout: 300,
  workdir: meson.current_build_dir(),
)
