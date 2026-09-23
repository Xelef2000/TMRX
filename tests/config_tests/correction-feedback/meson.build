configure_file(input: 'top.v', output: 'top.v', copy: true)

foreach test_case : ['default', 'enabled', 'register']
  configure_file(input: test_case + '.toml', output: test_case + '.toml', copy: true)
  configure_file(input: test_case + '.ys', output: test_case + '.ys', copy: true)

  test(
    'correction_feedback_' + test_case,
    yosys,
    args: ['-ql', test_case + '.log', '-m', plugin_path, '-s', test_case + '.ys'],
    timeout: 60,
    workdir: meson.current_build_dir(),
    depends: tmrx,
    suite: 'config_tests',
  )
endforeach
