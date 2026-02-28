# TMRX: Triple Modular Redundancy Expansion for Yosys

> **Warning**: TMRX is currently in active development and **not yet ready for production use**. Some features may not work as expected or may not be implemented. The tool may produce broken netlists. Use with caution and always verify outputs.

TMRX is a Yosys plugin that automatically injects Triple Modular Redundancy (TMR) into digital designs. It provides fine-grained fault-tolerance control across your design hierarchy, allowing you to mix Logic TMR and Full Module TMR strategies within the same design.

TMRX is intended for safety-critical, radiation-tolerant, and high-reliability FPGA/ASIC flows where selective redundancy is required.

## Table of Contents

- [Getting Started](#getting-started)
  - [Using Docker (Recommended)](#using-docker-recommended)
  - [Building from Source](#building-from-source)
- [Quick Start](#quick-start)
- [TMR Strategies](#tmr-strategies)
  - [Logic TMR](#logic-tmr)
  - [Full Module TMR](#full-module-tmr)
  - [Choosing a Strategy](#choosing-a-strategy)
- [Yosys Integration](#yosys-integration)
  - [The tmrx_mark Pass](#the-tmrx_mark-pass)
  - [The tmrx Pass](#the-tmrx-pass)
  - [Synthesis Flow](#synthesis-flow)
- [Configuration](#configuration)
  - [Configuration File (TOML)](#configuration-file-toml)
  - [Configuration Precedence](#configuration-precedence)
  - [Global Configuration](#global-configuration)
  - [Module Groups](#module-groups)
  - [Per-Module Configuration](#per-module-configuration)
  - [Specific Instance Configuration](#specific-instance-configuration)
  - [Verilog Attribute Configuration](#verilog-attribute-configuration)
- [Configuration Reference](#configuration-reference)
  - [TMR Mode Options](#tmr-mode-options)
  - [Voter Options](#voter-options)
  - [Port Preservation](#port-preservation)
  - [Clock and Reset Handling](#clock-and-reset-handling)
  - [Flip-Flop Detection](#flip-flop-detection)
  - [Naming Options](#naming-options)
- [Error Detection](#error-detection)
- [Wire and Port Attributes](#wire-and-port-attributes)
- [Reserved Groups](#reserved-groups)
- [Common Pitfalls and Troubleshooting](#common-pitfalls-and-troubleshooting)
- [Examples](#examples)
- [Running Tests](#running-tests)

---

## Getting Started

### Using Docker (Recommended)

The easiest way to use TMRX is with the prebuilt Docker container that includes all dependencies:

```bash
docker pull ghcr.io/xelef2000/vlsi-toolbox:latest-amd64
docker run -it -v $(pwd):/workspace ghcr.io/xelef2000/vlsi-toolbox:latest-amd64
```

The container includes Yosys, yosys-slang, and all required build tools.

Repository: [https://github.com/Xelef2000/vlsi-toolbox](https://github.com/Xelef2000/vlsi-toolbox)

### Building from Source

#### Requirements

- **Yosys** (with `yosys-config` in your `PATH`)
- **Meson** build system
- **C++17-compatible compiler**
- **Git** (for fetching dependencies)
- **Make** (for building sub-dependencies)

#### Build Steps

```bash
# Clone the repository
git clone https://github.com/Xelef2000/TMRX.git
cd TMRX

# Configure build directory
meson setup build

# Build the plugin
meson compile -C build
```

The shared library `tmrx.so` will be generated in `build/`.

During the first build, Meson automatically clones:
- **yosys-slang**: SystemVerilog frontend for Yosys
- **IHP-Open-PDK**: Open-source PDK used for testing

---

## Quick Start

1. Create a configuration file `tmrx_config.toml`:

```toml
[global]
tmr_mode = "LogicTMR"
insert_voter_after_ff = true
clock_port_names = ["clk_i", "clk"]
reset_port_names = ["rst_ni", "rst_n"]
```

2. Create a Yosys synthesis script:

```yosys
# Read design
read_verilog my_design.v
hierarchy -check -top top

# Standard synthesis
proc; opt
techmap

# Mark cells for TMR (run after optimization, before technology mapping)
tmrx_mark

# Technology mapping
read_liberty -lib cells.lib
dfflibmap -liberty cells.lib
abc -liberty cells.lib

# Apply TMR
tmrx -c tmrx_config.toml

# Final optimization
opt -noff
techmap
dfflibmap -liberty cells.lib
abc -liberty cells.lib

# Output
write_verilog -noattr output.v
```

3. Run Yosys with the plugin:

```bash
yosys -m build/tmrx.so -s synth.ys
```

---

## TMR Strategies

TMRX supports two complementary TMR strategies that can be mixed within the same design.

### Logic TMR

Logic TMR triplicates the **internal logic** of a module while optionally preserving the original interface.

**What gets triplicated:**
- All combinational logic gates
- All wires and signals
- All flip-flops
- All internal connections

**How it works:**
1. Every wire and cell is duplicated with configurable suffixes (e.g., `_a`, `_b`, `_c`)
2. Majority voters are inserted after flip-flop outputs (configurable)
3. If `preserve_module_ports = true`, voters are inserted at outputs to maintain the original interface
4. If `preserve_module_ports = false`, the module interface is expanded with triplicated ports



### Full Module TMR

Full Module TMR creates a **wrapper module** containing three independent instances of the original module.

**What gets created:**
- A new wrapper module with the original module name
- The original module is renamed (with `_tmrx_worker` suffix)
- Three instances of the worker module inside the wrapper
- Optional input voters (before modules)
- Optional output voters (after modules)

**How it works:**
1. The original module is renamed and wrapped
2. Three instances are created with connections to wrapper ports
3. If `preserve_module_ports = false`, ports are triplicated
4. Voters can be inserted at inputs, outputs, or both



### Choosing a Strategy

| Consideration | Logic TMR | Full Module TMR |
|--------------|-----------|-----------------|
| Interface changes | Optional (with `preserve_module_ports`) | Optional (with `preserve_module_ports`) |
| Voter placement | After FFs, at outputs | At module boundaries |
| Area overhead | Generally lower | Slightly higher (wrapper overhead) |
| Black-box modules | Not applicable | Required |

---

## Yosys Integration

TMRX provides two Yosys passes that integrate into your synthesis flow.

### The tmrx_mark Pass

```yosys
tmrx_mark
```

The `tmrx_mark` pass scans selected modules and marks:
- **Flip-flops**: Records FF source locations for later identification after technology mapping
- **Submodule boundaries**: Marks modules that are instantiated within others

**When to run:** After heavy optimization (`proc`, `opt`, `techmap`) but **before** `abc` and `dfflibmap`.

This pass is important because technology mapping can change cell types, making it harder to identify flip-flops. The marking preserves this information for the `tmrx` pass.

### The tmrx Pass

```yosys
tmrx -c config.toml
```

The main TMR expansion pass. It:
1. Loads configuration from the TOML file
2. Processes modules in topological order (leaf modules first)
3. Applies the configured TMR strategy to each module
4. Inserts voters according to configuration

**Arguments:**
- `-c <file>`: Path to TOML configuration file (optional, uses defaults if not provided)

### Synthesis Flow

The recommended synthesis flow order:

```
1. Read design (read_verilog / read_slang)
2. Hierarchy resolution (hierarchy -check -top <top>)
3. Process elaboration (proc)
4. Optimization (opt)
5. Technology mapping (techmap)
6. ══════════════════════════════════════
7. tmrx_mark  ← Mark FFs and submodules
8. ══════════════════════════════════════
9. Read liberty file (read_liberty -lib)
10. FF mapping (dfflibmap -liberty)
11. Logic synthesis (abc -liberty)
12. ══════════════════════════════════════
13. tmrx -c config.toml  ← Apply TMR
14. ══════════════════════════════════════
15. Final optimization (opt -noff)
16. Re-run techmap, dfflibmap, abc for voters
17. Cleanup (opt_clean -purge, clean)
18. Output (write_verilog, write_json)
```

---

## Configuration

### Configuration File (TOML)

TMRX uses TOML format for configuration. The file is passed to the `tmrx` pass:

```yosys
tmrx -c tmrx_config.toml
```

If no configuration file is provided or the file cannot be loaded, TMRX uses default settings.

### Configuration Precedence

Configuration is resolved in layers, with later layers overriding earlier ones:

1. **Global defaults** (built-in hardcoded defaults)
2. **Global config** (`[global]` section in TOML)
3. **Group config** (`[group<name>]` sections)
4. **Module config** (`[module_<name>]` sections)
5. **Verilog attributes** (`(* tmrx_* *)` on modules)
6. **Specific instance config** (`["specific_module_<name>"]` sections) — **highest priority**

This layered approach allows you to set sensible defaults globally and override them for specific modules or instances.

### Global Configuration

The `[global]` section sets default values for all modules:

```toml
[global]
tmr_mode = "LogicTMR"
tmr_voter = "Default"
preserve_module_ports = false
insert_voter_after_ff = true
insert_voter_before_ff = false
clock_port_names = ["clk_i", "clk"]
reset_port_names = ["rst_ni", "rst_n"]
expand_clock = false
expand_reset = false
logic_path_1_suffix = "_a"
logic_path_2_suffix = "_b"
logic_path_3_suffix = "_c"
```

### Module Groups

Groups allow you to apply the same configuration to multiple modules. First, assign modules to groups:

```toml
[module_groups]
uart_rx = "safety_critical"
uart_tx = "safety_critical"
debug_controller = "non_critical"
test_module = "non_critical"
```

Then define the group configurations:

```toml
[groupsafety_critical]
tmr_mode = "FullModuleTMR"
tmr_mode_full_module_insert_voter_after_modules = true
preserve_module_ports = false

[groupnon_critical]
tmr_mode = "None"
```

**Note:** Group names are prefixed with `group` in the section header (e.g., `[groupsafety_critical]` for group `safety_critical`).

### Per-Module Configuration

Configure specific modules by name using the `module_` prefix:

```toml
[module_alu]
tmr_mode = "LogicTMR"
expand_reset = true
insert_voter_after_ff = true

[module_register_file]
tmr_mode = "FullModuleTMR"
preserve_module_ports = true
```

### Specific Instance Configuration

When using yosys-slang, parameterized modules are often uniquified with names like `module$hierarchy.path`. To configure a specific instance:

```toml
["specific_module_submodule$top.cpu.u_alu"]
tmr_mode = "FullModuleTMR"
preserve_module_ports = true
```

**Important:** Specific module configurations must include a `$` in the name. Use this for uniquified module names from yosys-slang.

### Verilog Attribute Configuration

You can also configure TMR directly in your Verilog source using attributes:

```verilog
(* tmrx_tmr_mode = "FullModuleTMR" *)
(* tmrx_preserve_module_ports = "1" *)
(* tmrx_insert_voter_after_ff = "1" *)
module critical_module (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire [7:0] data_i,
    output wire [7:0] data_o
);
    // ...
endmodule
```

**Supported module attributes:**

| Attribute | Values | Description |
|-----------|--------|-------------|
| `tmrx_tmr_mode` | `"None"`, `"LogicTMR"`, `"FullModuleTMR"` | TMR strategy |
| `tmrx_tmr_voter` | `"Default"` | Voter type |
| `tmrx_preserve_module_ports` | `"0"`, `"1"` | Keep original interface |
| `tmrx_insert_voter_before_ff` | `"0"`, `"1"` | Voters before FFs (planned) |
| `tmrx_insert_voter_after_ff` | `"0"`, `"1"` | Voters after FFs |
| `tmrx_expand_clock` | `"0"`, `"1"` | Triplicate clock |
| `tmrx_expand_rst` | `"0"`, `"1"` | Triplicate reset |
| `tmrx_assign_to_group` | `"group_name"` or `"group1;group2"` | Assign to group(s) |
| `tmrx_clock_port_name` | `"clk"` or `"clk1;clk2"` | Clock port names |
| `tmrx_rst_port_name` | `"rst"` or `"rst1;rst2"` | Reset port names |

**Note:** For list values in attributes, use semicolon (`;`) as separator.

---

## Configuration Reference

### TMR Mode Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `tmr_mode` | String | `"LogicTMR"` | The TMR strategy to apply |

**Values:**
- `"None"`: No TMR applied to this module
- `"LogicTMR"`: Triplicate internal logic with voters after FFs
- `"FullModuleTMR"`: Create wrapper with three module instances

### Voter Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `tmr_voter` | String | `"Default"` | Voter implementation to use |
| `insert_voter_after_ff` | Bool | `true` | Insert voters after flip-flop outputs |
| `insert_voter_before_ff` | Bool | `false` | Insert voters before flip-flop inputs (**planned, not yet implemented**) |
| `tmr_mode_full_module_insert_voter_before_modules` | Bool | `false` | Insert voters at module inputs (Full Module TMR) |
| `tmr_mode_full_module_insert_voter_after_modules` | Bool | `true` | Insert voters at module outputs (Full Module TMR) |

**Voter types:**
- `"Default"`: Simple majority voter using AND/OR gates. Future versions may support alternative voter implementations (e.g., with different error detection characteristics).

The default voter implements:
- **Output**: `y = (a & b) | (a & c) | (b & c)` (majority function)
- **Error**: `err = (a ^ b) | (b ^ c)` (any mismatch detected)

### Port Preservation

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `preserve_module_ports` | Bool | `false` | Keep original module interface unchanged |

When `preserve_module_ports = true`:
- The module interface remains identical to the original
- Input signals are fanned out to all three redundant paths
- Output signals pass through voters to produce a single output
- Useful for drop-in replacement compatibility
- Allows mixing TMR and non-TMR modules in the same hierarchy

When `preserve_module_ports = false`:
- Input and output ports are triplicated (with suffixes like `_a`, `_b`, `_c`)
- Clock and reset ports may be shared or triplicated based on `expand_clock`/`expand_reset`
- Parent modules must connect to all three port variants

### Clock and Reset Handling

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `clock_port_names` | List | `["clk_i"]` | Names of clock ports |
| `reset_port_names` | List | `["rst_ni"]` | Names of reset ports |
| `expand_clock` | Bool | `false` | Triplicate the clock network |
| `expand_reset` | Bool | `false` | Triplicate the reset network |

**Clock/Reset identification:**
- Ports matching names in `clock_port_names` are treated as clocks
- Ports matching names in `reset_port_names` are treated as resets
- Alternatively, use wire attributes `(* tmrx_clk_port *)` or `(* tmrx_rst_port *)`

**Expansion behavior:**
- When `expand_clock = false`: All three redundant paths share the same clock
- When `expand_clock = true`: Clock is triplicated along with other signals
- Same logic applies to reset signals

**Recommendation:** Generally keep `expand_clock = false` and `expand_reset = false` unless you have specific requirements for clock/reset tree redundancy.

### Flip-Flop Detection

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ff_cells` | List | `[]` | Additional cell types to treat as flip-flops |
| `additional_ff_cells` | List | `[]` | More FF cells (additive, doesn't override inherited) |
| `excluded_ff_cells` | List | `[]` | Cell types to exclude from FF treatment |

TMRX automatically detects Yosys built-in FF types. These options allow fine-tuning:

- `ff_cells`: Extends auto-detection with additional cell types
- `additional_ff_cells`: Same as `ff_cells`, but designed for layered configs — adding cells here won't override `ff_cells` inherited from a parent config (e.g., global or group)
- `excluded_ff_cells`: Removes specific cells from FF treatment, useful when auto-detection or inherited config includes cells you don't want treated as FFs

**Example:**

```toml
[global]
ff_cells = ["sg13g2_dfrbp_1"]

[module_special]
additional_ff_cells = ["MY_CUSTOM_FF"]
excluded_ff_cells = ["sg13g2_dfrbp_1"]
```

### Naming Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `logic_path_1_suffix` | String | `"_a"` | Suffix for first redundant path |
| `logic_path_2_suffix` | String | `"_b"` | Suffix for second redundant path |
| `logic_path_3_suffix` | String | `"_c"` | Suffix for third redundant path |

These suffixes are appended to wire and cell names when triplicating logic.

---

## Error Detection

TMRX can aggregate error signals from all voters to a single output port for monitoring.

### Defining an Error Sink

Mark an output port with the `tmrx_error_sink` attribute:

```verilog
module my_module (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire [7:0] data_i,
    output wire [7:0] data_o,
    (* tmrx_error_sink *)
    output wire err_o
);
    // ...
endmodule
```

### How It Works

1. Each voter generates an error signal when inputs don't match
2. All error signals within the module are OR'd together
3. The combined error is connected to the `tmrx_error_sink` port
4. If submodules also have error sinks, those are propagated up

### Behavior Without Error Sink

If no `tmrx_error_sink` is defined:
- Voters still generate error signals internally
- Error signals are not connected (left floating)
- No warning is generated

**Recommendation:** Define an error sink for production designs to enable fault monitoring.

---

## Wire and Port Attributes

In addition to module attributes, TMRX recognizes attributes on wires/ports:

| Attribute | Description |
|-----------|-------------|
| `tmrx_clk_port` | Mark this wire as a clock signal |
| `tmrx_rst_port` | Mark this wire as a reset signal |
| `tmrx_error_sink` | Mark this output as the error aggregation point |

**Example:**

```verilog
module my_module (
    (* tmrx_clk_port *)
    input  wire clk,

    (* tmrx_rst_port *)
    input  wire reset_n,

    (* tmrx_error_sink *)
    output wire tmr_error
);
```

These attributes can be used instead of or in addition to the `clock_port_names` and `reset_port_names` configuration options.

---

## Reserved Groups

TMRX has built-in reserved groups that are automatically applied in certain situations:

### `black_box_module`

**Applied automatically when:**
- Module has the `blackbox` attribute
- Module contains memories (`has_memories`)
- Module contains processes (`has_processes`)

**Default behavior:**
- `tmr_mode = "FullModuleTMR"`

Black-box modules cannot have their internals modified, so Full Module TMR is the only applicable strategy.

### `cdc_module`

**Intended for:** Clock Domain Crossing modules

**Default behavior:**
- `tmr_mode = "FullModuleTMR"`

CDC modules typically require careful handling and should be treated as units.

**Note:** These groups are reserved. While the configuration can be read, the automatic assignment behavior should not be relied upon to be overridden.

---

## Common Pitfalls and Troubleshooting

### 1. Running tmrx_mark at the Wrong Time

**Problem:** FFs are not detected, voters not inserted correctly.

**Solution:** Run `tmrx_mark` after `techmap` but before `dfflibmap` and `abc`:

```yosys
techmap
tmrx_mark          # ← Correct position
read_liberty -lib cells.lib
dfflibmap -liberty cells.lib
abc -liberty cells.lib
```

### 2. Forgetting to Re-run Technology Mapping After TMR

**Problem:** Voter modules remain as abstract cells in final netlist.

**Solution:** Run `techmap`, `dfflibmap`, and `abc` again after `tmrx`:

```yosys
tmrx -c config.toml
opt -noff
techmap
dfflibmap -liberty cells.lib
abc -liberty cells.lib
```

### 3. Clock/Reset Not Recognized

**Problem:** Clock or reset signals are being triplicated unexpectedly.

**Solution:** Ensure port names match the configuration:

```toml
clock_port_names = ["clk_i", "clk", "clock"]
reset_port_names = ["rst_ni", "rst_n", "reset_n", "rstn"]
```

Or use wire attributes:

```verilog
(* tmrx_clk_port *) input wire my_unusual_clock_name,
```

### 4. Specific Module Config Not Applied

**Problem:** Configuration for a specific instance is ignored.

**Solution:** Ensure the name includes `$` and matches the uniquified name exactly:

```toml
# Wrong - no $, will be treated as module config
[module_submodule]

# Correct - includes hierarchy path
["specific_module_submodule$top.u_sub"]
```

Check Yosys output for the exact uniquified module names.

### 5. Group Configuration Not Taking Effect

**Problem:** Module assigned to group but group settings not applied.

**Solution:** Check group section naming. The section must be `[group<name>]` (no underscore):

```toml
[module_groups]
my_module = "critical"

# Wrong
[group_critical]

# Correct
[groupcritical]
```

### 6. Error Signal Not Connected

**Problem:** Error output remains unconnected or constant.

**Solution:** Ensure the error sink is marked and is an output port:

```verilog
(* tmrx_error_sink *)
output wire err_o   // Must be output, not input or wire
```

### 7. Broken Netlist Output

**Problem:** Generated netlist has connectivity issues or doesn't simulate correctly.

**Note:** TMRX is still in development. If you encounter broken netlists:
1. Try simplifying your design
2. Check if the issue is specific to certain TMR modes
3. Report the issue with a minimal reproducing example

---

## Examples

### Minimal Counter with Logic TMR

**counter.v:**
```verilog
module counter (
    input  wire clk_i,
    input  wire rst_ni,
    input  wire en_i,
    output reg [7:0] count_o,
    (* tmrx_error_sink *)
    output wire err_o
);

always @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni)
        count_o <= 8'd0;
    else if (en_i)
        count_o <= count_o + 1'd1;
end

endmodule
```

**config.toml:**
```toml
[global]
tmr_mode = "LogicTMR"
insert_voter_after_ff = true
preserve_module_ports = true
clock_port_names = ["clk_i"]
reset_port_names = ["rst_ni"]
```

### Hierarchical Design with Mixed Strategies

**config.toml:**
```toml
[global]
tmr_mode = "LogicTMR"
clock_port_names = ["clk_i", "clk"]
reset_port_names = ["rst_ni", "rst_n"]

# Critical control logic uses Full Module TMR
[module_groups]
control_unit = "critical"
alu = "critical"

[groupcritical]
tmr_mode = "FullModuleTMR"
tmr_mode_full_module_insert_voter_after_modules = true
preserve_module_ports = false

# Debug module excluded from TMR
[module_debug_controller]
tmr_mode = "None"

# Specific instance with preserved ports for compatibility
["specific_module_uart$top.u_uart"]
tmr_mode = "FullModuleTMR"
preserve_module_ports = true
```

---

## Running Tests

TMRX includes a comprehensive test suite:

```bash
# Run all tests
meson test -C build

# List available tests
meson test -C build --list

# Run a specific test
meson test -C build <test_name>

# Run tests with verbose output
meson test -C build -v
```

Test categories:
- `config_tests/`: Configuration parsing tests
- `ref_tests/`: Reference design tests (counter, croc SoC)
- `ihp_tests/`: IHP PDK integration tests
- `sub_module_instantiation_tests/`: Parametric tests for TMR mode combinations

---

## yosys-slang Integration

While TMRX works with standard `read_verilog`, using [yosys-slang](https://github.com/povik/yosys-slang) is recommended for:

- **SystemVerilog support**: Full SV2017 language support
- **Better hierarchy handling**: Proper module uniquification for parameterized modules
- **Cleaner elaboration**: More predictable module naming for `specific_module_` configs

**Example with yosys-slang:**

```yosys
read_slang --top top design.sv \
    --compat-mode \
    --keep-hierarchy \
    --allow-use-before-declare \
    --ignore-unknown-modules
```

yosys-slang is automatically cloned and built as part of the TMRX build process.

---

## License

TMRX is licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.
