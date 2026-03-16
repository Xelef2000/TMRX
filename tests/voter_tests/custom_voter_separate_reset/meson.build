sep_reset_conf = configuration_data()
sep_reset_conf.set('PDK_ROOT', pdk_abs_path)

configure_file(
  input: 'custom_voter_separate_reset.ys.in',
  output: 'custom_voter_separate_reset.ys',
  configuration: sep_reset_conf,
)

configure_file(input: 'top.v',            output: 'top.v',            copy: true)
configure_file(input: 'stateful_voter.v', output: 'stateful_voter.v', copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)

test(
  'custom_voter_separate_reset',
  yosys,
  args: [
    '-ql', 'custom_voter_separate_reset.log',
    '-m', slang_plugin_path,
    '-m', plugin_path,
    '-s', 'custom_voter_separate_reset.ys',
  ],
  timeout: 300,
  workdir: meson.current_build_dir(),
  depends: [slang_build, tmrx],
  suite: 'voter_tests',
)
