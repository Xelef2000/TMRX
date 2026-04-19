configure_file(input: 'top.v',             output: 'top.v',             copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)
configure_file(input: 'auto_error_port.ys', output: 'auto_error_port.ys', copy: true)
configure_file(input: 'check_auto_error_port.sh', output: 'check_auto_error_port.sh', copy: true)

test(
  'auto_error_port',
  find_program('bash'),
  args: ['check_auto_error_port.sh', yosys.full_path(), plugin_path],
  timeout: 60,
  workdir: meson.current_build_dir(),
  depends: [tmrx],
  suite: 'config_tests',
)
