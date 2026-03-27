configure_file(input: 'top.v', output: 'top.v', copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)
configure_file(input: 'child_error_port_name.ys', output: 'child_error_port_name.ys', copy: true)

test(
  'child_error_port_name',
  yosys,
  args: [
    '-ql', 'child_error_port_name.log',
    '-m', plugin_path,
    '-s', 'child_error_port_name.ys',
  ],
  timeout: 60,
  workdir: meson.current_build_dir(),
  depends: [tmrx],
  suite: 'config_tests',
)
