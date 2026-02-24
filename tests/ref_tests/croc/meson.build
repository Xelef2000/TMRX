croc_dir = meson.current_build_dir() / 'croc'

croc_commit = '737c7d4459cf0ce8b77862f8b11e41731fa84594'
croc_url = 'https://github.com/pulp-platform/croc.git'

clone_croc = custom_target(
  'clone_croc',
  output: 'croc_clone.stamp',
  command: [
    'sh',
    '-c', '''
    set -e

    CROC_DIR="'''
    + croc_dir
    + '''"
    COMMIT="'''
    + croc_commit
    + '''"

    if [ -d "$CROC_DIR/.git" ]; then
        echo "[INFO] Existing croc repo found"

        CURRENT_COMMIT=$(git -C "$CROC_DIR" rev-parse HEAD 2>/dev/null || echo "invalid")

        if [ "$CURRENT_COMMIT" != "$COMMIT" ]; then
            echo "[INFO] Commit mismatch ($CURRENT_COMMIT != $COMMIT), recloning"
            rm -rf "$CROC_DIR"
        else
            echo "[INFO] Commit matches, reusing repository"
        fi
    else
        echo "[INFO] croc directory missing or not a git repo"
        rm -rf "$CROC_DIR"
    fi

    if [ ! -d "$CROC_DIR/.git" ]; then
        echo "[INFO] Cloning croc"
        git clone --recursive "'''
    + croc_url
    + '''" "$CROC_DIR"
        git -C "$CROC_DIR" checkout "$COMMIT"
    fi

    git -C "$CROC_DIR" submodule update --init --recursive

    touch "'''
    + meson.current_build_dir() / 'croc_clone.stamp'
    + '''"
    ''',
  ],
)

croc_conf = configuration_data()
croc_conf.set('SLANG_PLUGIN', slang_plugin_path)
croc_conf.set('TMRX_PLUGIN', plugin_path)
croc_conf.set('TMRX_CONFIG', meson.current_build_dir() / 'tmrx_config.toml')

configure_file(
  input: 'croc_yosys_synthesis.tcl.in',
  output: 'croc_yosys_synthesis.tcl',
  configuration: croc_conf,
)

configure_file(
  input: 'tmrx_config.toml',
  output: 'tmrx_config.toml',
  copy: true,
)

replace_script = custom_target(
  'replace_croc_yosys_script',
  output: 'replace_script.stamp',
  depends: clone_croc,
  command: [
    'cp',
    meson.current_build_dir() / 'croc_yosys_synthesis.tcl',
    croc_dir / 'yosys/scripts/yosys_synthesis.tcl',
    '&&',
    'touch',
    '@OUTPUT@',
  ],
)

test(
  'croc_full_integration',
  find_program('bash'),
  args: [croc_dir / 'yosys/run_synthesis.sh', '--synth'],
  workdir: croc_dir / 'yosys',
  timeout: 1800,
  depends: [
    replace_script,
    slang_build,
    tmrx,
  ],
  env: [
    'TMRX_CONFIG=' + (meson.current_build_dir() / 'tmrx_config.toml'),
  ],
)
