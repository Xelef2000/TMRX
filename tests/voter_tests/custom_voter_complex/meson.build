complex_conf = configuration_data()
complex_conf.set('PDK_ROOT', pdk_abs_path)

configure_file(input: 'custom_voter_complex.ys.in', output: 'custom_voter_complex.ys', configuration: complex_conf)

configure_file(input: 'acc_top.v',        output: 'acc_top.v',        copy: true)
configure_file(input: 'stateful_voter.v', output: 'stateful_voter.v', copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)

test(
  'custom_voter_acc',
  yosys,
  args: [
    '-ql', 'custom_voter_acc.log',
    '-m', slang_plugin_path,
    '-m', plugin_path,
    '-s', 'custom_voter_complex.ys',
  ],
  timeout: 300,
  workdir: meson.current_build_dir(),
  depends: [slang_build, tmrx],
)
