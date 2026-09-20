configure_file(input: 'top.v', output: 'top.v', copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)
configure_file(input: 'auto_error_propagation.ys', output: 'auto_error_propagation.ys', copy: true)

test(
  'auto_error_propagation',
  yosys,
  args: [
    '-ql', 'auto_error_propagation.log',
    '-m', plugin_path,
    '-s', 'auto_error_propagation.ys',
  ],
  timeout: 60,
  workdir: meson.current_build_dir(),
  depends: [tmrx],
  suite: 'config_tests',
)
