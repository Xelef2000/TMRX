test2_conf = configuration_data()
test2_conf.set('PDK_ROOT', pdk_abs_path)

configure_file(
  input: 'ihp_synth_unflattened.ys.in',
  output: 'ihp_synth_unflattened.ys',
  configuration: test2_conf
)

configure_file(
  input: '../ihp_dummy.v',
  output: 'ihp_dummy.v',
  copy: true
)

test('ihp_synthesis_2',
  yosys,
  args: [
    '-ql', 'ihp_synth_unflattened.log',
    '-m', plugin_path,
    '-s', 'ihp_synth_unflattened.ys'
  ],
  timeout: 300,
  workdir: meson.current_build_dir()
)