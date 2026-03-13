import json
import os
import subprocess
import sys

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <netlist.json>", file=sys.stderr)
    sys.exit(1)

with open(sys.argv[1]) as f:
    nl = json.load(f)


def quote(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


lines = ["digraph hierarchy {", "  node [shape=box];"]

for mod_name, mod in nl["modules"].items():
    lines.append(f"  {quote(mod_name)};")
    for cell_name, cell in mod["cells"].items():
        cell_type = cell["type"]
        if cell_type in nl["modules"]:
            lines.append(
                f"  {quote(mod_name)} -> {quote(cell_type)} [label={quote(cell_name)}];"
            )

lines.append("}")

out_base = sys.argv[1].rsplit(".", 1)[0] + "_hierarchy"
dot_path = out_base + ".dot"
svg_path = out_base + ".svg"

with open(dot_path, "w") as f:
    f.write("\n".join(lines))

subprocess.run(["dot", "-Tsvg", "-o", svg_path, dot_path], check=True)
os.remove(dot_path)
print(f"Written to {svg_path}")
