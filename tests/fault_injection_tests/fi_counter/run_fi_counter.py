#!/usr/bin/env python3
"""End-to-end fault injection test for fi_counter.

Pipeline:
  1. Yosys: synthesise TMR version  (fi_counter_tmr.v)
  2. patch_vl_macros.py: rename escaped voter module names so Verilator
     can parse the generated Verilog
  3. Verilator: elaborate plain model and TMR model
  4. g++: compile plain testbench (sanity check)
  5. g++: compile TMR testbench   (fault injection)
  6. Run both testbenches

Faults are injected directly via Verilator's internal root-module struct
(no vrtlmod dependency).

Exit code is the number of unmasked faults (0 = all faults masked).
"""

import argparse
import re
import shutil
import subprocess
import sys
import pathlib
import tempfile


def run(cmd, *, cwd=None, check=True, label=None):
    if label:
        print(f"[{label}]", " ".join(str(c) for c in cmd))
    result = subprocess.run(
        [str(c) for c in cmd],
        cwd=str(cwd) if cwd else None,
        capture_output=False,
    )
    if check and result.returncode != 0:
        print(f"ERROR: command failed (exit {result.returncode})", file=sys.stderr)
        sys.exit(result.returncode)
    return result


def _patch_voter_names(verilog_path: pathlib.Path) -> None:
    """Replace escaped Verilog voter identifiers with short plain names.

    Yosys emits voter module names like:
        \\tmrx_voter_..._$auto$file.cc:298:func$N_..._w1
    These escaped identifiers contain '$', ':' and '.' which cause Verilator
    to hash the name differently for the definition vs. the instance reference,
    producing "Can't resolve module reference" errors.
    """
    content = verilog_path.read_text()
    pat = re.compile(r'\\tmrx_voter_\S+')
    name_map: dict[str, str] = {}
    counter = 0

    def _replace(m: re.Match) -> str:
        nonlocal counter
        orig = m.group(0)
        if orig not in name_map:
            name_map[orig] = f"tmrx_voter_{counter}"
            counter += 1
        return name_map[orig]

    patched = pat.sub(_replace, content)
    if patched != content:
        verilog_path.write_text(patched)
        print(f"  renamed {len(name_map)} escaped voter module name(s) in {verilog_path.name}")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--yosys",        required=True)
    p.add_argument("--tmrx-so",      required=True)
    p.add_argument("--slang-so",     required=True)
    p.add_argument("--input-v",      required=True)
    p.add_argument("--top",          required=True)
    p.add_argument("--tmrx-config",  required=True)
    p.add_argument("--patch-py",     required=True)
    p.add_argument("--tb-cpp",       required=True)
    p.add_argument("--verilator",    default="verilator")
    p.add_argument("--duration",     type=int, default=50)
    p.add_argument("--num-faults",   type=int, default=10)
    p.add_argument("--mode",         choices=["tmr-fi", "plain-fi", "tmr-sanity"], default="tmr-fi",
                   help="tmr-fi: run fault injection on TMR model (expected to pass); "
                        "plain-fi: run fault injection on plain model (expected to fail); "
                        "tmr-sanity: run TMR model without fault injection (verifies testbench setup)")
    p.add_argument("--workdir",      default=None,
                   help="Persistent work directory (created if absent). "
                        "Defaults to a temp dir that is kept on failure.")
    args = p.parse_args()

    # Resolve absolute paths so subprocess calls work from any cwd.
    yosys       = pathlib.Path(args.yosys).resolve()
    tmrx_so     = pathlib.Path(args.tmrx_so).resolve()
    slang_so    = pathlib.Path(args.slang_so).resolve()
    input_v     = pathlib.Path(args.input_v).resolve()
    tmrx_config = pathlib.Path(args.tmrx_config).resolve()
    patch_py    = pathlib.Path(args.patch_py).resolve()
    tb_cpp      = pathlib.Path(args.tb_cpp).resolve()
    top         = args.top

    if args.workdir:
        workdir = pathlib.Path(args.workdir)
        workdir.mkdir(parents=True, exist_ok=True)
        keep = True
    else:
        workdir = pathlib.Path(tempfile.mkdtemp(prefix="fi_counter_"))
        keep = False

    vl_include = pathlib.Path("/usr/local/share/verilator/include")

    obj_plain = workdir / "obj_plain"
    obj_tmr   = workdir / "obj_tmr"
    tmr_v     = workdir / f"{top}_tmr.v"

    try:
        # ----------------------------------------------------------------
        # 1. Yosys: synthesise TMR version
        # ----------------------------------------------------------------
        synth_ys = workdir / "synth.ys"
        synth_ys.write_text(
            f"plugin -i {slang_so}\n"
            f"plugin -i {tmrx_so}\n"
            f"read_verilog {input_v}\n"
            f"hierarchy -top {top}\n"
            f"proc; opt\n"
            f"tmrx_mark\n"
            f"tmrx -c {tmrx_config}\n"
            f"opt -noff\n"
            f"write_verilog -noattr {tmr_v}\n"
        )
        run([yosys, "-ql", workdir / "synth.log", "-s", synth_ys], label="yosys/tmr")

        # ----------------------------------------------------------------
        # 2. Rename escaped voter module names so Verilator can parse them
        # ----------------------------------------------------------------
        _patch_voter_names(tmr_v)

        # ----------------------------------------------------------------
        # 3. Verilator: elaborate plain model
        # ----------------------------------------------------------------
        obj_plain.mkdir(exist_ok=True)
        run([
            args.verilator, "--cc", input_v,
            "--top-module", top, "--Mdir", obj_plain,
        ], label="verilator/plain")

        # ----------------------------------------------------------------
        # 4. Verilator: elaborate TMR model
        # ----------------------------------------------------------------
        obj_tmr.mkdir(exist_ok=True)
        run([
            args.verilator, "--cc", tmr_v,
            "--top-module", top, "--Mdir", obj_tmr,
        ], label="verilator/tmr")

        # Patch VL_IN*/VL_OUT* macros in TMR headers (vrtlmod workaround
        # is no longer needed but the macro patch is still required so that
        # the header declares ports as concrete types instead of macros).
        tmr_headers = list(obj_tmr.glob("*.h"))
        run([sys.executable, patch_py] + tmr_headers, label="patch_vl_macros")

        # ----------------------------------------------------------------
        # 5. Compile plain testbench (sanity check, no fault injection)
        # ----------------------------------------------------------------
        tb_plain_bin = workdir / "tb_plain"
        plain_cpps = sorted(obj_plain.glob(f"V{top}*.cpp"))
        run([
            "g++", "-std=c++17",
            f"-I{vl_include}",
            f"-I{obj_plain}",
            tb_cpp,
        ] + plain_cpps + [
            vl_include / "verilated.cpp",
            "-o", tb_plain_bin,
        ], label="g++/plain")

        # ----------------------------------------------------------------
        # 6. Compile TMR testbench (direct register access, no vrtlmod)
        # ----------------------------------------------------------------
        tb_tmr_bin = workdir / "tb_tmr"
        tmr_cpps = sorted(obj_tmr.glob(f"V{top}*.cpp"))
        run([
            "g++", "-std=c++17",
            f"-I{vl_include}",
            f"-I{obj_tmr}",
            "-DTMR",
            tb_cpp,
        ] + tmr_cpps + [
            vl_include / "verilated.cpp",
            "-o", tb_tmr_bin,
        ], label="g++/tmr")

        # ----------------------------------------------------------------
        # 7. Compile plain fault-injection testbench
        # ----------------------------------------------------------------
        tb_plain_fi_bin = workdir / "tb_plain_fi"
        run([
            "g++", "-std=c++17",
            f"-I{vl_include}",
            f"-I{obj_plain}",
            "-DPLAIN_FI",
            tb_cpp,
        ] + plain_cpps + [
            vl_include / "verilated.cpp",
            "-o", tb_plain_fi_bin,
        ], label="g++/plain-fi")

        # ----------------------------------------------------------------
        # 8. Compile TMR sanity testbench (no fault injection, TMR model)
        # ----------------------------------------------------------------
        tb_tmr_sanity_bin = workdir / "tb_tmr_sanity"
        run([
            "g++", "-std=c++17",
            f"-I{vl_include}",
            f"-I{obj_tmr}",
            "-DTMR_SANITY",
            tb_cpp,
        ] + tmr_cpps + [
            vl_include / "verilated.cpp",
            "-o", tb_tmr_sanity_bin,
        ], label="g++/tmr-sanity")

        # ----------------------------------------------------------------
        # 10. Sanity-check plain model (no fault injection)
        # ----------------------------------------------------------------
        rc = run([tb_plain_bin,
                  "--duration",   str(args.duration),
                  "--num-faults", str(args.num_faults)],
                 check=False, label="run/plain-sanity").returncode
        if rc != 0:
            print("ERROR: plain model sanity check failed", file=sys.stderr)
            sys.exit(rc)

        # ----------------------------------------------------------------
        # 11. Run selected experiment and propagate its exit code
        # ----------------------------------------------------------------
        if args.mode == "tmr-fi":
            rc = run([tb_tmr_bin,
                      "--duration",   str(args.duration),
                      "--num-faults", str(args.num_faults)],
                     check=False, label="run/tmr-fi").returncode
        elif args.mode == "tmr-sanity":
            rc = run([tb_tmr_sanity_bin,
                      "--duration",   str(args.duration),
                      "--num-faults", str(args.num_faults)],
                     check=False, label="run/tmr-sanity").returncode
        else:  # plain-fi
            rc = run([tb_plain_fi_bin,
                      "--duration",   str(args.duration),
                      "--num-faults", str(args.num_faults)],
                     check=False, label="run/plain-fi").returncode
        sys.exit(rc)

    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        if not keep:
            print(f"Work directory kept for inspection: {workdir}", file=sys.stderr)
            keep = True
        sys.exit(1)
    finally:
        if not keep:
            shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    main()
