verilog_files = [
  'verilog_sources/ihp_dummy.v',
  'verilog_sources/ihp_dummy_clock_out.v',
  'verilog_sources/ihp_dummy_connected_and_processed_err.v',
  'verilog_sources/ihp_dummy_connected_err.v',
  'verilog_sources/ihp_dummy_no_err.v',
]

tmr_modes = ['"FullModuleTMR"', '"LogicTMR"']
bool_vals  = ['true', 'false']

sby_template  = files('equiv.sby.in')
toml_template = files('tmrx_config.toml.in')

foreach v_file : verilog_files
  v_name = v_file.split('/')[-1].split('.')[0] + '.v'

  configure_file(input: v_file, output: v_name, copy: true)

  foreach g_tmr : tmr_modes
    foreach g_preserv : bool_vals
      foreach g_expand_clk : bool_vals
        foreach g_expand_rst : bool_vals
          foreach m_tmr : tmr_modes
            foreach m_preserv : bool_vals
              foreach m_expand_clk : bool_vals
                foreach m_expand_rst : bool_vals

                  test_name = f'sby_@v_name@_g_@g_tmr@_@g_preserv@_@g_expand_clk@_@g_expand_rst@_m_@m_tmr@_@m_preserv@_@m_expand_clk@_@m_expand_rst@'.replace('"', '')

                  conf_data = configuration_data()
                  conf_data.set('G_TMR_MODE',  g_tmr)
                  conf_data.set('G_PRESERV',   g_preserv)
                  conf_data.set('G_EXPAND_CLK', g_expand_clk)
                  conf_data.set('G_EXPAND_RST', g_expand_rst)
                  conf_data.set('M_TMR_MODE',  m_tmr)
                  conf_data.set('M_PRESERV',   m_preserv)
                  conf_data.set('M_EXPAND_CLK', m_expand_clk)
                  conf_data.set('M_EXPAND_RST', m_expand_rst)

                  toml_filename = f'@test_name@_config.toml'
                  configure_file(
                    input: toml_template,
                    output: toml_filename,
                    configuration: conf_data,
                  )

                  sby_conf = configuration_data()
                  sby_conf.set('SLANG_SO',     slang_plugin_path)
                  sby_conf.set('TMRX_SO',      plugin_path)
                  sby_conf.set('VERILOG_FILE',  v_name)
                  sby_conf.set('TOML_FILE',     toml_filename)

                  sby_filename = f'@test_name@.sby'
                  configure_file(
                    input: sby_template,
                    output: sby_filename,
                    configuration: sby_conf,
                  )

                  test(
                    test_name,
                    sby,
                    args: ['-f', sby_filename],
                    timeout: 300,
                    workdir: meson.current_build_dir(),
                    depends: [slang_build, tmrx],
                    suite: 'formal_equiv_tests',
                  )

                endforeach
              endforeach
            endforeach
          endforeach
        endforeach
      endforeach
    endforeach
  endforeach
endforeach
