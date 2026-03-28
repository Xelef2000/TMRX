# TMRX: Triple Modular Redundancy Expansion for Yosys

> **Warning**: TMRX is currently in active development and **not yet ready for production use**. Some features may not work as expected or may not be implemented. The tool may produce broken netlists. Use with caution and always verify outputs.

TMRX is a Yosys plugin that automatically injects Triple Modular Redundancy (TMR) into digital designs. It provides fine-grained fault-tolerance control across your design hierarchy, allowing you to mix Logic TMR and Full Module TMR strategies within the same design.

TMRX is intended for safety-critical, radiation-tolerant, and high-reliability FPGA/ASIC flows where selective redundancy is required.

## Documentation

Full documentation is available at **https://xelef2000.github.io/TMRX/**

## Quick Install

```bash
docker pull ghcr.io/xelef2000/vlsi-toolbox:latest-amd64
docker run -it -v $(pwd):/workspace ghcr.io/xelef2000/vlsi-toolbox:latest-amd64
```

Or build from source:

```bash
git clone https://github.com/Xelef2000/TMRX.git
cd TMRX
meson setup build
meson compile -C build
```

## License

TMRX is licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.
