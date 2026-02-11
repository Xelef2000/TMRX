configure_file(input: 'full_adder.v', output: 'full_adder.v', copy: true)

configure_file(input: 'xor.v', output: 'xor.v', copy: true)

configure_file(input: 'flip-flop.v', output: 'flip-flop.v', copy: true)

configure_file(input: 'flip-flop-neg.v', output: 'flip-flop-neg.v', copy: true)

configure_file(input: 'flip-flop-multi.v', output: 'flip-flop-multi.v', copy: true)

current_dir = meson.current_build_dir()

test(
  'adder',
  yosys,
  args: [
    '-ql', 'test4.log',
    '-m', plugin_path,
    'full_adder.v',
    '-p', 'synth; tmrx -report_bits',
  ],
  workdir: current_dir,
)

test_files = [
  'test.v',
  'full_adder.v',
  'xor.v',
  'flip-flop.v',
  'flip-flop-neg.v',
  'flip-flop-multi.v',
]

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
    configuration: file_conf,
  )

  test(
    'synth_' + basename,
    yosys,
    args: [
      '-ql', 'synth_' + basename + '.log',
      '-m', plugin_path,
      '-s', synth_script,
      '-v', '10',
    ],
    workdir: current_dir,
    timeout: 300,
  )
endforeach
