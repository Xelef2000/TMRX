configure_file(
  input: 'test.v',
  output: 'test.v',
  copy: true
)

configure_file(
  input: 'full_adder.v',
  output: 'full_adder.v',
  copy: true
)


current_dir = meson.current_build_dir()

test('basic', yosys, 
  args: ['-ql', 'test1.log', '-m', plugin_path, 'test.v', '-p', 'prep; tmrx'],
  workdir: current_dir
)

test('opt', yosys, 
  args: ['-ql', 'test2.log', '-m', plugin_path, 'test.v', '-p', 'prep; opt; tmrx'],
  workdir: current_dir
)

test('report_bits', yosys, 
  args: ['-ql', 'test3.log', '-m', plugin_path, 'test.v', '-p', 'synth; tmrx -report_bits'],
  workdir: current_dir
)

test('adder', yosys, 
  args: ['-ql', 'test4.log', '-m', plugin_path, 'full_adder.v', '-p', 'synth; tmrx -report_bits'],
  workdir: current_dir
)

test_files = ['test.v', 'full_adder.v']

synth_conf = configuration_data()
synth_conf.set('PDK_ROOT', pdk_abs_path)

foreach vfile : test_files
  script_name = vfile.replace('.v', '_synth.ys')
  basename = vfile.replace('.v', '')
  
  file_conf = configuration_data()
  file_conf.set('PDK_ROOT', pdk_abs_path)
  file_conf.set('INPUT', vfile)
  file_conf.set('BASENAME', basename)
  
  synth_script = configure_file(
    input: 'synth_template.ys.in',
    output: script_name,
    configuration: file_conf
  )
  
  test('synth_' + basename, yosys,
    args: ['-ql', 'synth_' + basename + '.log', '-m', plugin_path, '-s', synth_script],
    workdir: current_dir,
    timeout: 300
  )
endforeach