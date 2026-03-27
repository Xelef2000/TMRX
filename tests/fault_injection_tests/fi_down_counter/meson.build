python = find_program('python3', required: true)
verilator = find_program('verilator', required: true)

fi_down_counter_src_dir  = meson.current_source_dir()
fi_down_counter_work_dir = meson.current_build_dir() / 'workdir'

# Reuse the generic runner and macro-patch scripts from fi_counter.
_fi_runner = meson.project_source_root() / 'tests/fault_injection_tests/fi_counter/run_fi_counter.py'
_fi_patch  = meson.project_source_root() / 'tests/fault_injection_tests/fi_counter/patch_vl_macros.py'

_common_args = [
    _fi_runner,
    '--yosys', yosys.full_path(),
    '--tmrx-so', plugin_path,
    '--slang-so', slang_plugin_path,
    '--input-v', fi_down_counter_src_dir / 'fi_down_counter.v',
    '--top', 'fi_down_counter',
    '--tmrx-config', fi_down_counter_src_dir / 'tmrx_config.toml',
    '--patch-py', _fi_patch,
    '--tb-cpp', fi_down_counter_src_dir / 'tb.cpp',
    '--verilator', verilator.full_path(),
    '--duration', '100',
    '--num-faults', '4000000',
]

# TMR model sanity check: no fault injection, verifies the testbench setup is correct.
test(
    'fi_down_counter_tmr_sanity',
    python,
    args: _common_args + ['--mode', 'tmr-sanity', '--workdir', fi_down_counter_work_dir / 'tmr-sanity'],
    timeout: 300,
    workdir: meson.current_build_dir(),
    depends: [slang_build, tmrx],
    suite: 'fault_injection_tests',
)

# TMR model: all single-bit faults should be masked by the majority voters.
test(
    'fi_down_counter_tmr_fault_injection',
    python,
    args: _common_args + ['--mode', 'tmr-fi', '--workdir', fi_down_counter_work_dir / 'tmr-fi'],
    timeout: 300,
    workdir: meson.current_build_dir(),
    depends: [slang_build, tmrx],
    suite: 'fault_injection_tests',
)

# Plain model: no voters, so every injected fault corrupts the output.
# This test is expected to fail (should_fail: true).
test(
    'fi_down_counter_plain_fault_injection',
    python,
    args: _common_args + ['--mode', 'plain-fi', '--workdir', fi_down_counter_work_dir / 'plain-fi'],
    timeout: 300,
    workdir: meson.current_build_dir(),
    depends: [slang_build, tmrx],
    suite: 'fault_injection_tests',
    should_fail: true,
)
