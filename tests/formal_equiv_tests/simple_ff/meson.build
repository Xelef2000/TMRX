ff_equiv_conf = configuration_data()
ff_equiv_conf.set('TMRX_SO', plugin_path)
ff_equiv_conf.set('SLANG_SO', slang_plugin_path)

configure_file(
  input: 'equiv.sby.in',
  output: 'equiv.sby',
  configuration: ff_equiv_conf,
)

configure_file(input: 'simple_ff.v',       output: 'simple_ff.v',       copy: true)
configure_file(input: 'tmrx_config.toml',  output: 'tmrx_config.toml',  copy: true)

test(
  'simple_ff_logic_tmr_equiv',
  sby,
  args: ['-f', 'equiv.sby'],
  timeout: 300,
  workdir: meson.current_build_dir(),
  depends: [slang_build, tmrx],
  suite: 'formal_equiv_tests',
)
