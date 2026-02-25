# TMRX: Triple Modular Redundancy Expansion for Yosys

**TMRX** is a plugin for Yosys that automatically injects **Triple Modular Redundancy (TMR)** into digital designs.

It enables **fine-grained fault-tolerance control** across your hierarchy, allowing you to mix *Logic TMR* and *Full Module TMR* strategies within the same design.

TMRX is intended for safety-critical, radiation-tolerant, and high-reliability FPGA/ASIC flows where selective redundancy is required.

---

# Overview

TMRX transforms an existing RTL/netlist by:

* Triplicating logic or module instances
* Inserting majority voters
* Optionally replicating clock/reset trees
* Preserving or expanding module interfaces
* Allowing per-instance configuration

It integrates directly into a standard Yosys synthesis flow.

---

# Features

## Logic TMR

* Triplicates **internal combinational logic, wires, and flip-flops**
* Keeps the original module interface (optional)
* Suitable for flattened or lower-level designs

## Full Module TMR

* Instantiates **three independent copies** of a module
* Optional output voter insertion
* Clean hierarchical redundancy

## Fine-Grained Configuration

Configure TMR at multiple levels:

* Global default
* Module group
* Module type
* Specific uniquified instance
* Inline Verilog attributes

## Automatic Voter Insertion

* Insert voters after flip-flops
* Insert voters before flip-flops (Logic TMR)
* Insert voters at module outputs (Full Module TMR)

## Clock & Reset Handling

* Share or replicate clock networks
* Share or replicate reset networks
* Explicit clock/reset port specification

---

# Requirements

To build and use TMRX:

* **Yosys** (with `yosys-config` in your `PATH`)
* **Meson** (build system)
* **C++17-compatible compiler**
* **Git** (for submodules)
* **Make** (for sub-dependencies)

---

## Prebuilt Environment (Docker)

A ready-to-use container is available:

* Image: `ghcr.io/xelef2000/vlsi-toolbox`
* Repository: [https://github.com/Xelef2000/vlsi-toolbox](https://github.com/Xelef2000/vlsi-toolbox)

---

# Building

TMRX uses Meson.

```bash
# Configure build directory
meson setup build

# Build plugin
meson compile -C build
```

The shared library `tmrx.so` will be generated in `build/`.

---

# Usage

TMRX is typically used in two stages:

1. **Mark important cells (optional but recommended)**
2. **Execute TMR expansion**

---

## Example Yosys Script

```yosys
read_slang --top top ihp_dummy.v \
        --compat-mode --keep-hierarchy \
        --allow-use-before-declare --ignore-unknown-modules

hierarchy -check -top top

proc; opt; write_verilog -noattr 02_opt.v

techmap
write_verilog -noattr 03_techmap.v


tmrx_mark

read_liberty -lib /run/host/home/felix/Documents/Projects/TMRX/IHP-Open-PDK/ihp-sg13g2/libs.ref/sg13g2_stdcell/lib/sg13g2_stdcell_typ_1p20V_25C.lib


dfflibmap -liberty /run/host/home/felix/Documents/Projects/TMRX/IHP-Open-PDK/ihp-sg13g2/libs.ref/sg13g2_stdcell/lib/sg13g2_stdcell_typ_1p20V_25C.lib
write_verilog -noattr 05_dff.v

abc -liberty /run/host/home/felix/Documents/Projects/TMRX/IHP-Open-PDK/ihp-sg13g2/libs.ref/sg13g2_stdcell/lib/sg13g2_stdcell_typ_1p20V_25C.lib
write_verilog -noattr 06_gates.v

tmrx -c tmrx_config.toml
write_verilog -noattr 07_tmrx.v
opt -noff
techmap
dfflibmap -liberty /run/host/home/felix/Documents/Projects/TMRX/IHP-Open-PDK/ihp-sg13g2/libs.ref/sg13g2_stdcell/lib/sg13g2_stdcell_typ_1p20V_25C.lib
abc -liberty /run/host/home/felix/Documents/Projects/TMRX/IHP-Open-PDK/ihp-sg13g2/libs.ref/sg13g2_stdcell/lib/sg13g2_stdcell_typ_1p20V_25C.lib -D 1000


opt_clean -purge

clean
write_verilog -noattr 08_final.v
write_json 08_final.json
show -format svg -prefix final_netlist top
show -format dot -prefix final_netlist top


stat -liberty /run/host/home/felix/Documents/Projects/TMRX/IHP-Open-PDK/ihp-sg13g2/libs.ref/sg13g2_stdcell/lib/sg13g2_stdcell_typ_1p20V_25C.lib

```

---

# The `tmrx_mark` Pass

`tmrx_mark` scans selected modules and marks:

* Flip-flops
* Submodule boundaries

It should be run **after heavy optimization**, but before abc and dfflibmap.

---

# Configuration

Configuration is provided via a **TOML file** passed using:

```bash
tmrx -c tmrx_config.toml
```

## Precedence (Highest → Lowest)

1. Specific module instance
2. Verilog attributes
3. Module config
4. Group config
5. Global config

This allows precise control over redundancy behavior.

---

# Configuration Options

| Option                                            | Type   | Description                               |
| ------------------------------------------------- | ------ | ----------------------------------------- |
| `tmr_mode`                                        | String | `None`, `LogicTMR`, `FullModuleTMR`       |
| `tmr_voter`                                       | String | `Default` (currently only supported type) |
| `preserve_module_ports`                           | Bool   | Keep original interface (internal voters) |
| `insert_voter_after_ff`                           | Bool   | Insert voter after each FF output         |
| `insert_voter_before_ff`                          | Bool   | Insert voter before FF input              |
| `clock_port_names`                                | List   | Clock port names (`["clk_i"]`)            |
| `reset_port_names`                                | List   | Reset port names (`["rst_ni"]`)           |
| `expand_clock`                                    | Bool   | Triplicate clock network                  |
| `expand_reset`                                    | Bool   | Triplicate reset network                  |
| `logic_path_1_suffix`                             | String | Suffix for redundant path A               |
| `logic_path_2_suffix`                             | String | Suffix for redundant path B               |
| `logic_path_3_suffix`                             | String | Suffix for redundant path C               |
| `tmr_mode_full_module_insert_voter_after_modules` | Bool   | Insert voters at outputs in FullModuleTMR |

---

# Example Configuration

```toml
# Global defaults
[global]
tmr_mode = "LogicTMR"
tmr_voter = "Default"
preserve_module_ports = false
insert_voter_after_ff = true
clock_port_names = ["clk_i", "clk"]
expand_clock = false
logic_path_1_suffix = "_a"
logic_path_2_suffix = "_b"
logic_path_3_suffix = "_c"

# Assign modules to groups
[module_groups]
uart_rx = "safety_critical"
debug_mod = "ignore"

# Group configuration
[groupsafety_critical]
tmr_mode = "FullModuleTMR"
tmr_mode_full_module_insert_voter_after_modules = true

[groupignore]
tmr_mode = "None"

# Per-module configuration
[module_alu]
tmr_mode = "LogicTMR"
expand_reset = true

# Specific uniquified instance
["specific_module_cpu$root.u_alu"]
tmr_mode = "FullModuleTMR"
preserve_module_ports = true
```

---

# Targeting Specific Instances

When using Slang-based frontends (`read_slang` / yosys-slang), parameterized modules are often **uniquified**.

Example uniquified name:

```
submodule$top.u_sub
```

To configure a single instance:

```toml
["specific_module_submodule$top.u_sub"]
tmr_mode = "FullModuleTMR"
```

This affects **only that instance**, not all modules of the same type.

---

# Verilog Attribute Configuration

TMR can also be controlled inline using attributes:

```verilog
(* tmrx_tmr_mode = "FullModuleTMR" *)
(* tmrx_preserve_module_ports = 1 *)
module my_critical_module (
    input clk,
    input rst_n,
    ...
);
```

Supported attributes mirror TOML keys using the `tmrx_` prefix:

* `(* tmrx_tmr_mode = "..." *)`
* `(* tmrx_assign_to_group = "group_name" *)`
* `(* tmrx_preserve_module_ports = 1 *)`
* `(* tmrx_insert_voter_after_ff = 1 *)`
* `(* tmrx_insert_voter_before_ff = 1 *)`
* `(* tmrx_expand_clock = 1 *)`
* etc.
 

---
