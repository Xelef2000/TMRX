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
const std::string cfg_tmr_mode_full_module_insert_voter_before_modules_attr_name =
    "\\tmrx_tmr_mode_full_module_insert_voter_before_modules";
const std::string cfg_tmr_mode_full_module_insert_voter_after_modules_attr_name =
    "\\tmrx_tmr_mode_full_module_insert_voter_after_modules";
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

const auto ATTRIBUTE_IS_PROPER_SUBMODULE = ID(tmrx_is_proper_submodule);

enum class TmrMode {
    None,
    FullModuleTMR,
    LogicTMR,
};

enum class TmrVoter { Default };

struct Config {

    TmrMode tmr_mode;


    TmrVoter tmr_voter;

    bool preserve_module_ports;

    bool insert_voter_before_ff;
    bool insert_voter_after_ff;

    bool tmr_mode_full_module_insert_voter_before_modules;
    bool tmr_mode_full_module_insert_voter_after_modules;

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
};

struct ConfigPart {


    std::optional<TmrMode> tmr_mode;

    std::optional<TmrVoter> tmr_voter;

    std::optional<bool> preserve_module_ports;

    std::optional<bool> insert_voter_before_ff;
    std::optional<bool> insert_voter_after_ff;

    std::optional<bool> tmr_mode_full_module_insert_voter_before_modules;
    std::optional<bool> tmr_mode_full_module_insert_voter_after_modules;

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
};

struct ConfigManager {
  private:
    void load_global_default_cfg();
    void load_default_groups_cfg();
    void validate_cfg();
    std::string get_string_attr_value_or(const Yosys::RTLIL::Module *mod, const std::string &attr,
                                         const std::string &def);
    bool get_bool_attr_value_or(const Yosys::RTLIL::Module *mod, const std::string &attr, bool def);
    int get_int_attr_value_or(const Yosys::RTLIL::Module *mod, const std::string &attr, int def);
    std::optional<std::string> get_string_attr_value(const Yosys::RTLIL::Module *mod,const std::string &attr);
    std::optional<bool> get_bool_attr_value(const Yosys::RTLIL::Module *mod, const std::string &attr);
    std::optional<int> get_int_attr_value(const Yosys::RTLIL::Module *mod, const std::string &attr);
    std::optional<std::vector<std::string>> get_string_list_attr_value(const Yosys::RTLIL::Module *mod,const std::string &attr);
    ConfigPart parse_config(const toml::value &t);
    ConfigPart parse_module_annotations(const Yosys::RTLIL::Module *mod);
    Config assemble_config(std::vector<ConfigPart> parts, Config def);


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
