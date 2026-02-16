test1_conf = configuration_data()
test1_conf.set('PDK_ROOT', pdk_abs_path)

configure_file(input: 'counter.ys.in', output: 'counter.ys', configuration: test1_conf)

configure_file(input: 'counter.v', output: 'counter.v', copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)

test(
  'basic_counter',
  yosys,
  args: ['-ql', 'counter.log', '-m', plugin_path, '-s', 'counter.ys'],
  timeout: 300,
  workdir: meson.current_build_dir(),
)
