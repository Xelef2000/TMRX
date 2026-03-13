import json
import os
import subprocess
import sys
from collections import Counter, deque

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <netlist.json> [root_module]", file=sys.stderr)
    sys.exit(1)

with open(sys.argv[1]) as f:
    nl = json.load(f)


def quote(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


# Build adjacency: only user-defined modules (no $ prefix)
user_modules = {name for name in nl["modules"] if not name.startswith("$")}

children = {}  # mod -> Counter of child module types
for mod_name in user_modules:
    counts = Counter()
    for cell in nl["modules"][mod_name]["cells"].values():
        ct = cell["type"]
        if ct in user_modules:
            counts[ct] += 1
    children[mod_name] = counts

# If root specified, only show reachable modules
if len(sys.argv) >= 3:
    root = sys.argv[2]
    if root not in user_modules:
        print(f"Module '{root}' not found. User modules:", file=sys.stderr)
        for m in sorted(user_modules):
            print(f"  {m}", file=sys.stderr)
        sys.exit(1)
    reachable = set()
    queue = deque([root])
    while queue:
        m = queue.popleft()
        if m in reachable:
            continue
        reachable.add(m)
        for child in children.get(m, {}):
            queue.append(child)
    user_modules = reachable

# Build DOT
lines = [
    "digraph hierarchy {",
    "  rankdir=TB;",
    "  node [shape=box, style=filled, fillcolor=lightyellow];",
]

for mod in sorted(user_modules):
    lines.append(f"  {quote(mod)};")
    for child, count in children.get(mod, {}).items():
        if child in user_modules:
            label = f"x{count}" if count > 1 else ""
            lines.append(f"  {quote(mod)} -> {quote(child)} [label={quote(label)}];")

lines.append("}")

out_base = sys.argv[1].rsplit(".", 1)[0] + "_hierarchy"
dot_path = out_base + ".dot"
svg_path = out_base + ".svg"

with open(dot_path, "w") as f:
    f.write("\n".join(lines))

subprocess.run(["dot", "-Tsvg", "-o", svg_path, dot_path], check=True)
os.remove(dot_path)
print(f"Written to {svg_path}")
print(
    f"  {len(user_modules)} modules, filtered out {len(nl['modules']) - len(user_modules)} internal modules"
)
