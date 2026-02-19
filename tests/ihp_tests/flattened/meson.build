test1_conf = configuration_data()
test1_conf.set('PDK_ROOT', pdk_abs_path)

configure_file(
  input: 'ihp_synth_flattened.ys.in',
  output: 'ihp_synth_flattened.ys',
  configuration: test1_conf,
)

configure_file(input: '../ihp_dummy.v', output: 'ihp_dummy.v', copy: true)
configure_file(input: '../tmrx_config.toml', output: 'tmrx_config.toml', copy: true)

test(
  'ihp_synth_flattenedesis_1',
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
