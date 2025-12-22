project(
  'stubnets',
  'cpp',
  default_options: ['cpp_std=c++17']
)

yosys_config = find_program('yosys-config', required: true)
yosys        = find_program('yosys', required: true)

stubnets = custom_target(
  'stubnets_so',
  input: 'stubnets.cc',
  output: 'stubnets.so',
  command: [
    yosys_config,
    '--exec',
    '--cxx',
    '--cxxflags',
    '--ldflags',
    '-shared',
    '-o', '@OUTPUT@',
    '@INPUT@',
    '--ldlibs'
  ],
  build_by_default: true
)

plugin_path = stubnets.full_path()

test_v = meson.project_source_root() / 'test.v'


test('basic',
  yosys,
  args: [
    '-ql', 'test1.log',
    '-m', plugin_path,
    test_v,
    '-p', 'stubnets'
  ]
)

test('opt',
  yosys,
  args: [
    '-ql', 'test2.log',
    '-m', plugin_path,
    test_v,
    '-p', 'opt; stubnets'
  ]
)

test('report_bits',
  yosys,
  args: [
    '-ql', 'test3.log',
    '-m', plugin_path,
    test_v,
    '-p', 'techmap; opt; stubnets -report_bits'
  ]
)