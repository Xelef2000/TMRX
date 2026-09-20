configure_file(input: 'top.v', output: 'top.v', copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)
configure_file(input: 'register_tmr.ys', output: 'register_tmr.ys', copy: true)

test(
  'register_tmr',
  yosys,
  args: ['-ql', 'register_tmr.log', '-m', plugin_path, '-s', 'register_tmr.ys'],
  timeout: 60,
  workdir: meson.current_build_dir(),
  depends: [tmrx],
  suite: 'config_tests',
)
