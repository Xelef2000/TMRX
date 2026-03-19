#!/usr/bin/env python3
"""Convert a Verilog flist's relative paths to absolute paths.

Usage: gen_abs_flist.py <input_flist> <output_flist> [base_dir]

base_dir: directory from which flist-internal relative paths are resolved
          (defaults to the flist file's own directory).
          Needed when the flist was generated to be used from a different CWD
          than where it lives (e.g. croc's flist lives in yosys/src/ but its
          paths are relative to yosys/).
"""
import sys
import pathlib

flist_path = pathlib.Path(sys.argv[1])
out_path   = pathlib.Path(sys.argv[2])
base       = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else flist_path.parent

# In distrobox environments the host filesystem is mounted under /run/host.
# Tools like yosys run inside the container and see /home/... paths, not
# /run/host/home/... paths.  Strip the prefix if present so generated
# paths are valid inside the container.
_HOST_PREFIX = pathlib.Path('/run/host')

def _to_container_path(p: pathlib.Path) -> pathlib.Path:
    try:
        return pathlib.Path('/') / p.relative_to(_HOST_PREFIX)
    except ValueError:
        return p

# Modules whose RTL bodies contain constructs not available in the formal flow
# (e.g. IHP SRAM macros with unsupported timing constructs) are excluded so
# that hierarchy auto-blackboxes them as opaque stubs.
_EXCLUDED_SRCS = {
    "tc_sram_impl.sv",    # instantiates RM_IHPSG13_* whose verilog breaks read_verilog
}

lines = []
for raw in flist_path.read_text().splitlines():
    line = raw.strip()
    if not line or line.startswith("//") or line.startswith("#"):
        lines.append(line)
    elif line.startswith("+incdir+"):
        rel = line[len("+incdir+"):]
        lines.append("+incdir+" + str(_to_container_path((base / rel).resolve())))
    elif line.startswith("+") or line.startswith("-"):
        lines.append(line)
    else:
        abs_path = _to_container_path((base / line).resolve())
        if abs_path.name in _EXCLUDED_SRCS:
            lines.append("# excluded: " + str(abs_path))
        else:
            lines.append(str(abs_path))

out_path.write_text("\n".join(lines) + "\n")
print("Generated", out_path)
