#!/usr/bin/env bash
set -euo pipefail

yosys_bin="$1"
plugin_path="$2"

"${yosys_bin}" -ql auto_error_port.log -m "${plugin_path}" -s auto_error_port.ys

grep -Fq "has preserve_module_ports enabled together with auto_error_port" auto_error_port.log
grep -Fq "Auto-created error port '\\tmrx_err_o' in module '\\top'" auto_error_port.log
