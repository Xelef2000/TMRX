# TMRX: Triple Modular Redundancy Expansion for Yosys

**TMRX** is a Yosys plugin designed to automatically inject Triple Modular Redundancy (TMR) into digital logic designs. It supports fine-grained configuration, allowing you to mix different TMR strategies (Logic TMR vs. Full Module TMR) across different parts of your design hierarchy.

## Features

* 
**Logic TMR:** Triplicates the internal logic, wires, and flip-flops of a module while maintaining the original interface (optional).


* **Full Module TMR:** Triplicates the module instantiation itself, running three distinct copies of the module working in parallel.
* 
**Flexible Configuration:** Configure TMR strategies globally, per module group, per module type, or per specific unique instance.


* 
**Voter Insertion:** Automatically inserts majority voters after flip-flops or module boundaries to correct faults.


* 
**Clock/Reset Handling:** Options to triplicate clock/reset trees or share them across redundant logic.



## Dependencies & Requirements

To build and use this plugin, you need the following:

* **Yosys:** (with `yosys-config` in your PATH).
* **Meson:** The build system used by this project.
* 
**C++ Compiler:** (supporting C++17 or later).


* **Git:** Required to fetch submodules.
* 
**Make:** Required for building sub-dependencies.



### Docker Container

For a pre-configured environment containing all necessary dependencies, you can use the **VLSI Toolbox** container:

* Image: `ghcr.io/xelef2000/vlsi-toolbox`
* Repo: [https://github.com/Xelef2000/vlsi-toolbox](https://github.com/Xelef2000/vlsi-toolbox)

## Building the Plugin

The project uses the Meson build system. It will automatically fetch and build dependencies like `yosys-slang` if they are not present.

```bash
# Setup the build directory
meson setup build

# Compile the plugin
meson compile -C build

```

This will produce `tmrx.so` in the build directory.

## Usage

### 1. Integration in Yosys Script

The TMRX flow typically involves two steps: marking the design (optional but recommended for identifying specific cells) and executing the expansion.

A typical synthesis script (`.ys`) looks like this:

```yosys
# ... read design files ...

# 1. Flatten the design (often required for Logic TMR context)
flatten

# 2. Mark Flip-Flops
# This pass scans modules and marks FF cells before they are heavily optimized
tmrx_mark

# ... standard synthesis/mapping (techmap, abc, dfflibmap) ...

# 3. Execute TMR Expansion
# Load the plugin (if not autoloaded) and run the pass with a config file
plugin -i tmrx.so
tmrx -c tmrx_config.toml

# ... final optimization and cleanup ...
opt -noff
clean
write_verilog final_tmr.v

```

### 2. The `tmrx_mark` Pass

The `tmrx_mark` pass scans selected modules for flip-flops and submodules. It is useful to run this early in the flow to identify cells that should be protected before synthesis transformations potentially obscure them.

## Configuration

Configuration is handled via a TOML file passed with the `-c` flag. The configuration system uses a hierarchy of precedence:

1. **Specific Module Config** (Highest priority)
2. **Verilog Attributes** (Inline in HDL)
3. **Module Config**
4. **Group Config**
5. **Global Config** (Lowest priority)

### Configuration Options

The following options can be set in the TOML file:

| Option | Type | Description |
| --- | --- | --- |
| `tmr_mode` | String | `None`, `LogicTMR` (triplicate internals), or `FullModuleTMR` (triplicate instantiation). |
| `tmr_voter` | String | `Default` (currently the only voter type). |
| `preserve_module_ports` | Bool | If `true`, the module interface remains unchanged (voters inside). If `false`, ports are triplicated. |
| `insert_voter_after_ff` | Bool | Insert a majority voter on the output of every triplicated Flip-Flop. |
| `insert_voter_before_ff` | Bool | Insert a voter before the Flip-Flop input (Logic TMR). |
| `clock_port_names` | List | Names of clock ports (e.g., `["clk_i"]`). |
| `reset_port_names` | List | Names of reset ports (e.g., `["rst_ni"]`). |
| `expand_clock` | Bool | If `true`, triplicates the clock network. If `false`, shares one clock. |
| `expand_reset` | Bool | If `true`, triplicates the reset network. |
| `logic_path_suffix` | String | Suffixes for redundant paths (e.g., `_a`, `_b`, `_c`). |
| `tmr_mode_full_module_insert_voter_after_modules` | Bool | In `FullModuleTMR`, insert voters on the outputs of the triplicated modules. |

### TOML Configuration Example (`tmrx_config.toml`)

```toml
# Default settings for the entire design
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

# Configuration for specific groups
[groupsafety_critical]
tmr_mode = "FullModuleTMR"
tmr_mode_full_module_insert_voter_after_modules = true

[groupignore]
tmr_mode = "None"

# Configuration for a specific Verilog module type
[module_alu]
tmr_mode = "LogicTMR"
expand_reset = true

# Configuration for a SPECIFIC INSTANCE (Uniquified)
# See note below regarding Slang uniquification
["specific_module_cpu$root.u_alu"]
tmr_mode = "FullModuleTMR"
preserve_module_ports = true

```

### Note on "Specific Modules"

When using frontends like **Slang** (via `read_slang` or `yosys-slang`), modules are often "uniquified" to handle parameters. This results in module names containing dollar signs and hierarchy paths (e.g., `submodule$top.u_sub`).

To configure a specific instance of a module without affecting all instances of that type, use the `specific_module_` prefix in your TOML file followed by the full uniquified name. In the TOML example above, `["specific_module_submodule$top.u_sub"]` targets exactly one instance.

### Verilog Attributes

You can also configure TMR directly in your Verilog source using attributes.

```verilog
(* tmrx_tmr_mode = "FullModuleTMR" *)
(* tmrx_tmr_voter = "Default" *)
(* tmrx_preserve_module_ports = 1 *)
module my_critical_module (
    // ...
);

```

Available attributes correspond to the TOML keys, prefixed with `tmrx_`:

* `(* tmrx_tmr_mode = "..." *)`
* `(* tmrx_assign_to_group = "group_name" *)`
* `(* tmrx_preserve_module_ports = 1 *)`
* `(* tmrx_insert_voter_after_ff = 1 *)`
* etc.
