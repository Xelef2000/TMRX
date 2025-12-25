project(
  'stubnets',
  'cpp',
  default_options: ['cpp_std=c++11']
)

fs = import('fs')
yosys_config = find_program('yosys-config', required: true)
yosys        = find_program('yosys', required: true)
git          = find_program('git', required: true)

pdk_dir_name = 'IHP-Open-PDK'
pdk_abs_path = meson.project_source_root() / pdk_dir_name

if not fs.is_dir(pdk_abs_path)
  message('Cloning IHP-Open-PDK...')
  run_command(
    git, 'clone', '--depth', '1', 
    'https://github.com/IHP-GmbH/IHP-Open-PDK.git', 
    pdk_abs_path,
    check: true
  )
endif

stubnets = custom_target(
  'stubnets_so',
  input: 'stubnets.cc',
  output: 'stubnets.so',
  command: [
    yosys_config, '--exec', '--cxx', '--cxxflags', '--ldflags',
    '-shared', '-o', '@OUTPUT@', '@INPUT@', '--ldlibs'
  ],
  build_by_default: true
)

plugin_path = stubnets.full_path()


subdir('tests/std_tests')
subdir('tests/ihp_tests')