# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

TMRX is a Yosys plugin that automatically injects Triple Modular Redundancy (TMR) into digital designs for fault-tolerant FPGA/ASIC flows. It supports two TMR strategies:
- **Logic TMR**: Triplicates internal combinational logic, wires, and flip-flops within a module
- **Full Module TMR**: Instantiates three independent copies of a module with optional output voters

## Build Commands

```bash
# Configure and build
meson setup build
meson compile -C build

# Run all tests
meson test -C build

# Run a specific test
meson test -C build <test_name>

# List available tests
meson test -C build --list
```

The plugin is built as `build/tmrx.so`. Dependencies (yosys-slang and IHP-Open-PDK) are cloned automatically during setup.

## Architecture

### Source Files (src/)

- **tmrx_pass.cc**: Main `tmrx` pass that orchestrates TMR expansion. Processes modules in topological order (leaf modules first) and dispatches to Logic TMR or Full Module TMR based on config.
- **mark_pass.cc**: `tmrx_mark` pass that scans and marks flip-flops and submodule boundaries. Should run after optimization but before abc/dfflibmap.
- **config_manager.cc**: Layered configuration system that merges scoped TOML tables, inline group memberships, Verilog attributes, and global defaults.
- **tmrx_logic_expansion.cc**: Implements Logic TMR - duplicates wires/cells with suffixes, connects submodule ports, inserts voters after FFs.
- **tmrx_mod_expansion.cc**: Implements Full Module TMR - creates wrapper module with three instances and optional input/output voters.
- **tmrx_utils.cc**: Helper functions for voter insertion, error signal handling, wire/cell classification.

### Configuration Precedence (highest to lowest)

1. Specific module instance config (`[specific_module."<name>"]`)
2. Verilog attributes (`(* tmrx_* *)`)
3. Module config (`[module.<name>]`)
4. Group config (`[group.<name>]`)
5. Global config (`[global]`)

### Key Types

- `TmrMode`: `None`, `LogicTMR`, `FullModuleTMR`
- `Config`: Full resolved configuration for a module
- `ConfigPart`: Partial configuration used in layering system

## Test Structure

Tests are organized under `tests/`:
- **config_tests/**: Configuration parsing tests
- **ref_tests/**: Reference design tests (counter, croc)
- **ihp_tests/**: IHP PDK integration tests (flattened/unflattened)
- **sub_module_instantiation_tests/**: Parametric tests for all TMR mode combinations

Tests use Yosys scripts (`.ys.in` templates) configured via Meson.

## Usage in Yosys

```yosys
# Load plugin
plugin -i /path/to/tmrx.so

# After synthesis, before abc/dfflibmap:
tmrx_mark

# After dfflibmap/abc:
tmrx -c config.toml
```

## External Dependencies

- **yosys-slang**: Cloned to `yosys-slang/` for SystemVerilog frontend
- **IHP-Open-PDK**: Cloned to `IHP-Open-PDK/` for standard cell library in tests
