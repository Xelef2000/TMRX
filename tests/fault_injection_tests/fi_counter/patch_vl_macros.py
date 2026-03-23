#!/usr/bin/env python3
"""Replace VL_IN*/VL_OUT*/VL_INOUT* port macros in Verilator 4.228 generated
headers with direct typed declarations that vrtlmod's clang parser can handle.

Verilator 4.228 uses macros like VL_IN8(clk_i,0,0) for port declarations.
These expand at the macro definition site (verilated_types.h), so the field's
source location is inside a system header. vrtlmod's get_source_code_str()
returns an empty string for such locations, causing a crash. Replacing the
macros with concrete declarations (CData/*0:0*/ clk_i) makes the source text
available at the usage site.
"""
import re, sys, pathlib

# Maps macro name → C++ base type (None = VlWide<words>)
_TYPE = {
    'VL_IN8':    'CData', 'VL_IN16':   'SData', 'VL_IN64':   'QData',
    'VL_IN':     'IData', 'VL_INW':    None,
    'VL_INOUT8': 'CData', 'VL_INOUT16':'SData', 'VL_INOUT64':'QData',
    'VL_INOUT':  'IData', 'VL_INOUTW': None,
    'VL_OUT8':   'CData', 'VL_OUT16':  'SData', 'VL_OUT64':  'QData',
    'VL_OUT':    'IData', 'VL_OUTW':   None,
}

_WIDE_PAT   = re.compile(r'(VL_(?:IN|OUT|INOUT)W)\(([^)]+)\)')
_SCALAR_PAT = re.compile(r'(VL_(?:IN|OUT|INOUT)(?:8|16|64)?)\(([^)]+)\)')

def patch(src: str) -> str:
    def _wide(m):
        name, msb, lsb, words = [x.strip() for x in m.group(2).split(',')]
        return f'VlWide<{words}>/*{msb}:{lsb}*/ {name}'

    def _scalar(m):
        macro = m.group(1)
        if _TYPE.get(macro) is None:
            return m.group(0)  # wide macro handled separately
        name, msb, lsb = [x.strip() for x in m.group(2).split(',')]
        return f'{_TYPE[macro]}/*{msb}:{lsb}*/ {name}'

    src = _WIDE_PAT.sub(_wide, src)
    src = _SCALAR_PAT.sub(_scalar, src)
    return src

if __name__ == '__main__':
    for path in map(pathlib.Path, sys.argv[1:]):
        original = path.read_text()
        patched  = patch(original)
        if patched != original:
            path.write_text(patched)
            print(f'Patched {path}')
