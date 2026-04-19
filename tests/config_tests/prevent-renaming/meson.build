configure_file(input: 'top.v', output: 'top.v', copy: true)
configure_file(input: 'default_tmrx_config.toml', output: 'default_tmrx_config.toml', copy: true)
configure_file(input: 'prevent_tmrx_config.toml', output: 'prevent_tmrx_config.toml', copy: true)
configure_file(input: 'emit_rtlil.ys', output: 'emit_rtlil.ys', copy: true)
configure_file(input: 'check_prevent_renaming.sh', output: 'check_prevent_renaming.sh', copy: true)

test(
  'prevent_renaming',
  find_program('bash'),
  args: ['check_prevent_renaming.sh', yosys.full_path(), plugin_path],
  timeout: 60,
  workdir: meson.current_build_dir(),
  depends: [tmrx],
  suite: 'config_tests',
)
