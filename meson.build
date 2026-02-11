project('tmrx', 'cpp', default_options: ['cpp_std=c++11'])

fs = import('fs')
yosys_config = find_program('yosys-config', required: true)
yosys = find_program('yosys', required: true)
git = find_program('git', required: true)

pdk_dir_name = 'IHP-Open-PDK'
pdk_abs_path = meson.project_source_root() / pdk_dir_name

if not fs.is_dir(pdk_abs_path)
  message('Cloning IHP-Open-PDK...')
  run_command(
    git,
    'clone',
    '--depth', '1',
    'https://github.com/IHP-GmbH/IHP-Open-PDK.git',
    pdk_abs_path,
    check: true,
  )
endif

tmrx = custom_target(
  'tmrx_so',
  input: 'tmrx.cc',
  output: 'tmrx.so',
  command: [
    yosys_config,
    '--exec',
    '--cxx',
    '--cxxflags',
    '--ldflags',
    '-shared',
    '-o', '@OUTPUT@',
    '@INPUT@',
    '--ldlibs',
  ],
  build_by_default: true,
)

plugin_path = tmrx.full_path()

subdir('tests/std_tests')
subdir('tests/ihp_tests')
subdir('tests/ref_tests')
