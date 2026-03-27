configure_file(input: 'top.v',             output: 'top.v',             copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)
configure_file(input: 'auto_error_port.ys', output: 'auto_error_port.ys', copy: true)

test(
  'auto_error_port',
  yosys,
  args: [
    '-ql', 'auto_error_port.log',
    '-m', plugin_path,
    '-s', 'auto_error_port.ys',
  ],
  timeout: 60,
  workdir: meson.current_build_dir(),
  depends: [tmrx],
  suite: 'config_tests',
)
