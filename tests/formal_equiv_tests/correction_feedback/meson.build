correction_feedback_conf = configuration_data()
correction_feedback_conf.set('TMRX_SO', plugin_path)

configure_file(
  input: 'correction_feedback.sby.in',
  output: 'correction_feedback.sby',
  configuration: correction_feedback_conf,
)
configure_file(input: 'top.v', output: 'top.v', copy: true)
configure_file(input: 'tmrx_config.toml', output: 'tmrx_config.toml', copy: true)
configure_file(input: 'formal_top.sv', output: 'formal_top.sv', copy: true)

test(
  'correction_feedback_proof',
  sby,
  args: ['-f', 'correction_feedback.sby'],
  timeout: 3600,
  workdir: meson.current_build_dir(),
  depends: tmrx,
  suite: 'formal_equiv_tests',
)
