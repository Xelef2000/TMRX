configure_file(
  input: 'test.v',
  output: 'test.v',
  copy: true
)

current_dir = meson.current_build_dir()

test('basic', yosys, 
  args: ['-ql', 'test1.log', '-m', plugin_path, 'test.v', '-p', 'tmrx'],
  workdir: current_dir
)

test('opt', yosys, 
  args: ['-ql', 'test2.log', '-m', plugin_path, 'test.v', '-p', 'opt; tmrx'],
  workdir: current_dir
)

test('report_bits', yosys, 
  args: ['-ql', 'test3.log', '-m', plugin_path, 'test.v', '-p', 'techmap; opt; tmrx -report_bits'],
  workdir: current_dir
)