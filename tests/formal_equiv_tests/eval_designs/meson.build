eval_equiv_cases = [
  ['counter_16', 'counter_16_formal_top.sv'],
  ['counter_32', 'counter_32_formal_top.sv'],
  ['fifo_buffer', 'fifo_formal_top.sv'],
  ['spi_controller', 'spi_formal_top.sv'],
]

foreach eval_case : eval_equiv_cases
  case_name = eval_case[0]
  formal_source = eval_case[1]
  top_file = case_name + '_top.v'
  config_file = case_name + '_tmrx_config.toml'
  formal_file = case_name + '_formal_top.sv'
  sby_file = case_name + '_equiv.sby'

  configure_file(input: case_name / 'top.v', output: top_file, copy: true)
  configure_file(input: case_name / 'tmrx_config.toml', output: config_file, copy: true)
  configure_file(input: formal_source, output: formal_file, copy: true)

  eval_equiv_conf = configuration_data()
  eval_equiv_conf.set('SLANG_SO', slang_plugin_path)
  eval_equiv_conf.set('TMRX_SO', plugin_path)
  eval_equiv_conf.set('TOP_FILE', top_file)
  eval_equiv_conf.set('CONFIG_FILE', config_file)
  eval_equiv_conf.set('FORMAL_FILE', formal_file)

  configure_file(
    input: 'equiv.sby.in',
    output: sby_file,
    configuration: eval_equiv_conf,
  )

  test(
    case_name + '_functional_equiv',
    sby,
    args: ['-f', sby_file],
    timeout: 3600,
    workdir: meson.current_build_dir(),
    depends: [slang_build, tmrx],
    suite: 'formal_equiv_tests',
  )
endforeach
