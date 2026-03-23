python = find_program('python3', required: true)
verilator = find_program('verilator', required: true)

fi_counter_src_dir = meson.current_source_dir()
fi_counter_work_dir = meson.current_build_dir() / 'workdir'

_common_args = [
    fi_counter_src_dir / 'run_fi_counter.py',
    '--yosys', yosys.full_path(),
    '--tmrx-so', plugin_path,
    '--slang-so', slang_plugin_path,
    '--input-v', fi_counter_src_dir / 'fi_counter.v',
    '--top', 'fi_counter',
    '--tmrx-config', fi_counter_src_dir / 'tmrx_config.toml',
    '--patch-py', fi_counter_src_dir / 'patch_vl_macros.py',
    '--tb-cpp', fi_counter_src_dir / 'tb.cpp',
    '--verilator', verilator.full_path(),
    '--duration', '100',
    '--num-faults', '4000000',
]

# TMR model: all single-bit faults should be masked by the majority voters.
test(
    'fi_counter_tmr_fault_injection',
    python,
    args: _common_args + ['--mode', 'tmr-fi', '--workdir', fi_counter_work_dir / 'tmr-fi'],
    timeout: 300,
    workdir: meson.current_build_dir(),
    depends: [slang_build, tmrx],
    suite: 'fault_injection_tests',
)

# Plain model: no voters, so every injected fault corrupts the output.
# This test is expected to fail (should_fail: true).
test(
    'fi_counter_plain_fault_injection',
    python,
    args: _common_args + ['--mode', 'plain-fi', '--workdir', fi_counter_work_dir / 'plain-fi'],
    timeout: 300,
    workdir: meson.current_build_dir(),
    depends: [slang_build, tmrx],
    suite: 'fault_injection_tests',
    should_fail: true,
)
