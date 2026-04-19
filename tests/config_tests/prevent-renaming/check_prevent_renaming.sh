#!/usr/bin/env bash
set -euo pipefail

yosys_bin="$1"
plugin_path="$2"

run_case() {
  local config_file="$1"
  local output_file="$2"
  sed \
    -e "s|CONFIG_FILE|${config_file}|g" \
    -e "s|OUTPUT_FILE|${output_file}|g" \
    emit_rtlil.ys > run.ys

  "${yosys_bin}" -q -m "${plugin_path}" -s run.ys
}

run_case default_tmrx_config.toml default.il
run_case prevent_tmrx_config.toml prevent.il

grep -Fq 'module \child_a' default.il
grep -Fq 'module \child_b' default.il
grep -Fq 'module \child_c' default.il

grep -Fq 'module \child' prevent.il
if grep -Fq 'module \child_a' prevent.il; then
  echo "unexpected branch-local child_a module when prevent_renaming=true" >&2
  exit 1
fi
if grep -Fq 'module \child_b' prevent.il; then
  echo "unexpected branch-local child_b module when prevent_renaming=true" >&2
  exit 1
fi
if grep -Fq 'module \child_c' prevent.il; then
  echo "unexpected branch-local child_c module when prevent_renaming=true" >&2
  exit 1
fi
