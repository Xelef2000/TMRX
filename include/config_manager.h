#ifndef TMRX_CONFIG_MANAGER_H
#define TMRX_CONFIG_MANAGER_H

#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include "toml11/toml.hpp"
#include <optional>
#include <string>
#include <vector>

YOSYS_NAMESPACE_BEGIN

YOSYS_NAMESPACE_END

const std::string cfg_group_prefix = "group";
const std::string cfg_module_prefix = "module_";
const std::string cfg_specific_module_prefix = "specific_module_";

const std::string cfg_group_assignment_attr_name = "\\tmrx_assign_to_group";
const std::string cfg_tmr_mode_attr_name = "\\tmrx_tmr_mode";
const std::string cfg_tmr_voter_attr_name = "\\tmrx_tmr_voter";
const std::string cfg_tmr_voter_safe_mode_attr_name = "\\tmrx_tmr_voter_safe_mode";
const std::string cfg_tmr_mode_full_module_insert_voter_before_modules_attr_name =
    "\\tmrx_tmr_mode_full_module_insert_voter_before_modules";
const std::string cfg_tmr_mode_full_module_insert_voter_after_modules_attr_name =
    "\\tmrx_tmr_mode_full_module_insert_voter_after_modules";
const std::string cfg_tmr_voter_file_attr_name = "\\tmrx_tmr_voter_file";
const std::string cfg_tmr_voter_module_attr_name = "\\tmrx_tmr_voter_module";
const std::string cfg_tmr_voter_clock_port_name_attr_name = "\\tmrx_tmr_voter_clock_port_name";
const std::string cfg_tmr_voter_reset_port_name_attr_name = "\\tmrx_tmr_voter_reset_port_name";
const std::string cfg_tmr_voter_clock_net_attr_name = "\\tmrx_tmr_voter_clock_net";
const std::string cfg_tmr_voter_reset_net_attr_name = "\\tmrx_tmr_voter_reset_net";
const std::string cfg_tmr_mode_full_module_insert_voter_on_clock_nets_attr_name =
        "\\tmrx_tmr_mode_full_module_insert_voter_on_clock_nets";
const std::string cfg_tmr_mode_full_module_insert_voter_on_reset_nets_attr_name =
        "\\tmrx_tmr_mode_full_module_insert_voter_on_reset_nets";
const std::string cfg_tmr_preserve_module_ports_attr_name = "\\tmrx_tmr_preserve_module_ports";
const std::string cfg_insert_voter_before_ff_attr_name = "\\tmrx_insert_voter_before_ff";
const std::string cfg_insert_voter_after_ff_attr_name = "\\tmrx_insert_voter_after_ff";
const std::string cfg_clock_port_name_attr_name = "\\tmrx_clock_port_name";
const std::string cfg_rst_port_name_attr_name = "\\tmrx_rst_port_name";
const std::string cfg_expand_clock_attr_name = "\\tmrx_expand_clock";
const std::string cfg_expand_rst_attr_name = "\\tmrx_expand_rst";
const std::string cfg_logic_path_1_suffix_attr_name = "\\tmrx_logic_path_1_suffix";
const std::string cfg_logic_path_2_suffix_attr_name = "\\tmrx_logic_path_2_suffix";
const std::string cfg_logic_path_3_suffix_attr_name = "\\tmrx_logic_path_3_suffix";
const std::string cfg_error_port_name_attr_name = "\\tmrx_error_port_name";

const auto ATTRIBUTE_IS_PROPER_SUBMODULE = ID(tmrx_is_proper_submodule);

enum class TmrMode {
    None,
    FullModuleTMR,
    LogicTMR,
};

enum class TmrVoter { Default, Custom };

// String conversion helpers
std::optional<TmrMode> parse_tmr_mode(const std::string &str);
std::string tmr_mode_to_string(TmrMode mode);
std::optional<TmrVoter> parse_tmr_voter(const std::string &str);
std::string tmr_voter_to_string(TmrVoter voter);

// TOML parsing helpers
template<typename T>
std::optional<T> toml_find_optional(const toml::value &t, const std::string &key);

std::optional<Yosys::pool<Yosys::RTLIL::IdString>>
toml_parse_idstring_pool(const toml::value &t, const std::string &key);

// Config assembly helper
template<typename T>
void apply_if_present(T &dest, const std::optional<T> &src);

// String formatting helpers
std::string pool_to_string(const Yosys::pool<Yosys::RTLIL::IdString> &pool);
std::string bool_to_string(bool val);

struct Config {

    TmrMode tmr_mode;

    TmrVoter tmr_voter;
    std::string tmr_voter_file;
    std::string tmr_voter_module;
    std::string tmr_voter_clock_port_name;  // voter's clock input port name
    std::string tmr_voter_reset_port_name;  // voter's reset input port name
    std::string tmr_voter_clock_net;        // parent wire to drive voter clock (default: first clock port)
    std::string tmr_voter_reset_net;        // parent wire to drive voter reset (default: first reset port)

    bool tmr_voter_safe_mode;

    bool preserve_module_ports;

    bool insert_voter_before_ff;
    bool insert_voter_after_ff;

    bool tmr_mode_full_module_insert_voter_before_modules;
    bool tmr_mode_full_module_insert_voter_after_modules;
    bool tmr_mode_full_module_insert_voter_on_clock_nets;
    bool tmr_mode_full_module_insert_voter_on_reset_nets;

    Yosys::pool<Yosys::RTLIL::IdString> clock_port_names;
    bool expand_clock;

    Yosys::pool<Yosys::RTLIL::IdString> reset_port_names;
    bool expand_reset;

    Yosys::pool<Yosys::RTLIL::IdString> ff_cells;
    Yosys::pool<Yosys::RTLIL::IdString> additional_ff_cells;
    Yosys::pool<Yosys::RTLIL::IdString> excluded_ff_cells;

    std::string logic_path_1_suffix;
    std::string logic_path_2_suffix;
    std::string logic_path_3_suffix;

    // Name of the output port that collects aggregated voter error signals.
    // Overrides (or supplements) the per-wire `(* tmrx_error_sink *)` attribute.
    // Empty string means "use attribute only".
    std::string error_port_name;
};

struct ConfigPart {


    std::optional<TmrMode> tmr_mode;

    std::optional<TmrVoter> tmr_voter;
    std::optional<std::string> tmr_voter_file;
    std::optional<std::string> tmr_voter_module;
    std::optional<std::string> tmr_voter_clock_port_name;
    std::optional<std::string> tmr_voter_reset_port_name;
    std::optional<std::string> tmr_voter_clock_net;
    std::optional<std::string> tmr_voter_reset_net;
    std::optional<bool> tmr_voter_safe_mode;

    std::optional<bool> preserve_module_ports;

    std::optional<bool> insert_voter_before_ff;
    std::optional<bool> insert_voter_after_ff;

    std::optional<bool> tmr_mode_full_module_insert_voter_before_modules;
    std::optional<bool> tmr_mode_full_module_insert_voter_after_modules;
    std::optional<bool> tmr_mode_full_module_insert_voter_on_clock_nets;
    std::optional<bool> tmr_mode_full_module_insert_voter_on_reset_nets;

    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> clock_port_names;
    std::optional<bool> expand_clock;

    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> reset_port_names;
    std::optional<bool> expand_reset;

    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> ff_cells;
    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> additional_ff_cells;
    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> excluded_ff_cells;

    std::optional<std::string> logic_path_1_suffix;
    std::optional<std::string> logic_path_2_suffix;
    std::optional<std::string> logic_path_3_suffix;

    std::optional<std::string> error_port_name;
};

struct ConfigManager {
  private:
    void load_global_default_cfg();
    void load_default_groups_cfg();
    void load_custom_voters(Yosys::RTLIL::Design *design);
    void validate_cfg(Yosys::RTLIL::Design *design);

    // Attribute parsing helpers
    std::string get_string_attr_value_or(const Yosys::RTLIL::Module *mod, const std::string &attr,
                                         const std::string &def);
    bool get_bool_attr_value_or(const Yosys::RTLIL::Module *mod, const std::string &attr, bool def);
    int get_int_attr_value_or(const Yosys::RTLIL::Module *mod, const std::string &attr, int def);
    std::optional<std::string> get_string_attr_value(const Yosys::RTLIL::Module *mod, const std::string &attr);
    std::optional<bool> get_bool_attr_value(const Yosys::RTLIL::Module *mod, const std::string &attr);
    std::optional<int> get_int_attr_value(const Yosys::RTLIL::Module *mod, const std::string &attr);
    std::optional<std::vector<std::string>> get_string_list_attr_value(const Yosys::RTLIL::Module *mod, const std::string &attr);
    std::optional<Yosys::pool<Yosys::RTLIL::IdString>>
    parse_attr_idstring_pool(const Yosys::RTLIL::Module *mod, const std::string &attr);

    // Config parsing and assembly
    ConfigPart parse_config(const toml::value &t);
    ConfigPart parse_module_annotations(const Yosys::RTLIL::Module *mod);
    Config assemble_config(std::vector<ConfigPart> parts, Config def);

    // Config assembly helpers
    void append_group_configs(
        std::vector<ConfigPart> &cfg_parts,
        const Yosys::dict<Yosys::RTLIL::IdString, std::vector<std::string>> &group_assignments,
        Yosys::RTLIL::IdString lookup_name,
        Yosys::RTLIL::IdString log_name);
    void append_config_if_present(
        std::vector<ConfigPart> &cfg_parts,
        const Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> &cfg_map,
        Yosys::RTLIL::IdString key);


    Config global_cfg;

    Yosys::dict<std::string, ConfigPart> group_cfg;
    Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> module_cfgs;
    Yosys::dict<Yosys::RTLIL::IdString, Config> final_module_cfgs;
    Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> specific_module_cfgs;
    Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> module_attr_cfgs;

  public:
    ConfigManager(Yosys::RTLIL::Design *design, const std::string &config_file);
    const Config *cfg(Yosys::RTLIL::Module *mod) const;
    std::string cfg_as_string(Yosys::RTLIL::Module *mod) const;
};

#endif
