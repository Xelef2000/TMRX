voter_conf = configuration_data()
voter_conf.set('PDK_ROOT', pdk_abs_path)

configure_file(input: 'custom_voter.ys.in', output: 'custom_voter.ys', configuration: voter_conf)

configure_file(input: 'counter.v',       output: 'counter.v',       copy: true)
configure_file(input: 'simple_voter.v',  output: 'simple_voter.v',  copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)

test(
  'custom_voter_counter',
  yosys,
  args: [
    '-ql', 'custom_voter_counter.log',
    '-m', slang_plugin_path,
    '-m', plugin_path,
    '-s', 'custom_voter.ys',
  ],
  timeout: 300,
  workdir: meson.current_build_dir(),
  depends: [slang_build, tmrx],
)
