croc_commit = '4ca88de5ce533c48bc2f1d77da57f5c8ff4859d1'
croc_url    = 'https://github.com/pulp-platform/croc.git'
croc_dir    = meson.current_build_dir() / 'croc'
croc_flist  = croc_dir / 'yosys' / 'src' / 'croc.flist'

clone_croc_formal = custom_target(
    'clone_croc_formal',
    output: 'croc_clone_formal.stamp',
    command: [
        'sh', '-c', '''
    set -e
    CROC_DIR="@0@"
    COMMIT="@1@"
    if [ -d "$CROC_DIR/.git" ]; then
        CURRENT=$(git -C "$CROC_DIR" rev-parse HEAD 2>/dev/null || echo invalid)
        if [ "$CURRENT" != "$COMMIT" ]; then
            echo "[INFO] Commit mismatch, recloning croc"
            rm -rf "$CROC_DIR"
        else
            echo "[INFO] croc already cloned at correct commit"
        fi
    else
        rm -rf "$CROC_DIR"
    fi
    if [ ! -d "$CROC_DIR/.git" ]; then
        echo "[INFO] Cloning croc"
        git clone --recursive "@2@" "$CROC_DIR"
        git -C "$CROC_DIR" checkout "$COMMIT"
    fi
    git -C "$CROC_DIR" submodule update --init --recursive
    touch "@3@"
    '''.format(
            croc_dir,
            croc_commit,
            croc_url,
            meson.current_build_dir() / 'croc_clone_formal.stamp',
        ),
    ],
)

# Generate an absolute-path version of croc.flist.
# sby runs yosys in <test>/src/ but slang resolves relative paths inside the
# flist relative to the working directory, not the flist's directory.
# Replacing all relative paths with absolute ones fixes this.
croc_flist_abs = meson.current_build_dir() / 'croc_abs.flist'
gen_flist = custom_target(
    'gen_croc_abs_flist',
    output: 'croc_abs.flist',
    depends: clone_croc_formal,
    command: [
        'python3',
        meson.current_source_dir() / 'gen_abs_flist.py',
        croc_flist,
        croc_flist_abs,
        croc_dir / 'yosys',   # flist paths are relative to yosys/, not yosys/src/
    ],
)

tc_sram_stub = meson.current_source_dir() / 'tc_sram_impl_stub.sv'

croc_conf = configuration_data()
croc_conf.set('SLANG_SO',       slang_plugin_path)
croc_conf.set('TMRX_SO',        plugin_path)
croc_conf.set('CROC_FLIST_ABS', croc_flist_abs)
croc_conf.set('TOML_FILE',      meson.current_build_dir() / 'tmrx_config.toml')
croc_conf.set('TC_SRAM_STUB',   tc_sram_stub)
croc_conf.set('SG13G2_STUBS',   meson.current_source_dir() / 'sg13g2_stubs.sv')

configure_file(
    input: 'equiv.sby.in',
    output: 'equiv.sby',
    configuration: croc_conf,
)

configure_file(
    input: 'tmrx_config.toml',
    output: 'tmrx_config.toml',
    copy: true,
)

test(
    'croc_soc_logic_tmr_equiv',
    sby,
    args: ['-f', 'equiv.sby'],
    timeout: 3600,
    workdir: meson.current_build_dir(),
    depends: [slang_build, tmrx, gen_flist],
    suite: 'formal_equiv_tests',
)
