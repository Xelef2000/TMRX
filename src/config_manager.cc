#include "config_manager.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include <string>

void ConfigManager::load_global_default_cfg() {
    global_cfg = {};

    global_cfg.tmr_mode = Config::TmrMode::LogicTMR;
    global_cfg.tmr_voter = Config::TmrVoter::Default;

    global_cfg.preserv_module_ports = false;

    global_cfg.insert_voter_before_ff = false;
    global_cfg.insert_voter_after_ff = true;

    global_cfg.tmr_mode_full_module_insert_voter_before_modules = false;
    global_cfg.tmr_mode_full_module_insert_voter_after_modules = true;

    global_cfg.clock_port_name = "clock_i";
    global_cfg.reset_port_name = "rst_ni";

    global_cfg.expand_clock = false;
    global_cfg.expand_reset = false;

    global_cfg.ff_cells = {};
    global_cfg.additional_ff_cells = {};
    global_cfg.excludet_ff_cells = {};

    global_cfg.logic_path_1_suffix = "_a";
    global_cfg.logic_path_2_suffix = "_b";
    global_cfg.logic_path_3_suffix = "_c";
}

void ConfigManager::load_default_groups_cfg() {
    // TODO: parmatrize
    group_cfg["black_box_module"] = Config(global_cfg);
    group_cfg["black_box_module"].tmr_mode = Config::TmrMode::FullModuleTMR;

    group_cfg["cdc_module"] = Config(global_cfg);
    group_cfg["cdc_module"].tmr_mode = Config::TmrMode::FullModuleTMR;

    // TODO: add top module
}

Config ConfigManager::parse_config(const toml::value &t, const Config &default_cfg) {
    Config cfg(default_cfg);

    std::string tmr_mode_str = toml::find_or<std::string>(t, "tmr_mode", "");

    // TODO: enum str parserser <- ->

    if (!tmr_mode_str.empty()) {
        if (tmr_mode_str == "None")
            cfg.tmr_mode = Config::TmrMode::None;
        else if (tmr_mode_str == "FullModuleTMR")
            cfg.tmr_mode = Config::TmrMode::FullModuleTMR;
        else if (tmr_mode_str == "LogicTMR")
            cfg.tmr_mode = Config::TmrMode::LogicTMR;
    }

    std::string tmr_voter_str = toml::find_or<std::string>(t, "tmr_voter", "");
    if (!tmr_voter_str.empty()) {
        if (tmr_voter_str == "Default")
            cfg.tmr_voter = Config::TmrVoter::Default;
    }

    cfg.preserv_module_ports =
        toml::find_or(t, "preserv_module_ports", default_cfg.preserv_module_ports);

    cfg.insert_voter_before_ff =
        toml::find_or(t, "insert_voter_before_ff", default_cfg.insert_voter_before_ff);
    cfg.insert_voter_after_ff =
        toml::find_or(t, "insert_voter_after_ff", default_cfg.insert_voter_after_ff);

    cfg.clock_port_name = toml::find_or(t, "clock_port_name", default_cfg.clock_port_name);
    cfg.expand_clock = toml::find_or(t, "expand_clock", default_cfg.expand_clock);

    cfg.reset_port_name = toml::find_or(t, "reset_port_name", default_cfg.reset_port_name);
    cfg.expand_reset = toml::find_or(t, "expand_reset", default_cfg.expand_reset);

    cfg.logic_path_1_suffix = toml::find_or(t, "logic_path_1_suffix", cfg.logic_path_1_suffix);
    cfg.logic_path_2_suffix = toml::find_or(t, "logic_path_2_suffix", cfg.logic_path_2_suffix);
    cfg.logic_path_3_suffix = toml::find_or(t, "logic_path_3_suffix", cfg.logic_path_3_suffix);

    for (auto &cell : toml::find_or<std::vector<std::string>>(t, "ff_cells", {})) {
        cfg.ff_cells.insert(Yosys::RTLIL::escape_id(cell));
    }

    for (auto &cell : toml::find_or<std::vector<std::string>>(t, "additional_ff_cells", {})) {
        cfg.additional_ff_cells.insert(Yosys::RTLIL::escape_id(cell));
    }

    for (auto &cell : toml::find_or<std::vector<std::string>>(t, "excludet_ff_cells", {})) {
        cfg.excludet_ff_cells.insert(Yosys::RTLIL::escape_id(cell));
    }

    return cfg;
}

Config ConfigManager::parse_module_annotations(const Yosys::RTLIL::Module *mod,
                                               const Config &default_cfg) {
    Config cfg(default_cfg);

    // TODO: move strings to header and factor out
    std::string tmr_mode_str = mod->get_string_attribute("tmrx_tmr_mode");
    if (!tmr_mode_str.empty()) {
        if (tmr_mode_str == "None")
            cfg.tmr_mode = Config::TmrMode::None;
        else if (tmr_mode_str == "FullModuleTMR")
            cfg.tmr_mode = Config::TmrMode::FullModuleTMR;
        else if (tmr_mode_str == "LogicTMR")
            cfg.tmr_mode = Config::TmrMode::LogicTMR;
    }

    std::string tmr_voter_str = mod->get_string_attribute("tmrx_tmr_voter");
    if (!tmr_voter_str.empty()) {
        if (tmr_voter_str == "Default")
            cfg.tmr_voter = Config::TmrVoter::Default;
    }

    std::string preserve_ports_str = mod->get_string_attribute("tmrx_tmr_preserve_module_ports");
    if (!preserve_ports_str.empty()) {
        cfg.preserv_module_ports = preserve_ports_str == "1";
    }

    std::string insert_voter_before_ff_str =
        mod->get_string_attribute("tmrx_insert_voter_before_ff");
    if (!insert_voter_before_ff_str.empty()) {
        cfg.insert_voter_before_ff = insert_voter_before_ff_str == "1";
    }

    std::string insert_voter_after_ff_str = mod->get_string_attribute("tmrx_insert_voter_after_ff");
    if (!insert_voter_after_ff_str.empty()) {
        cfg.insert_voter_after_ff = insert_voter_after_ff_str == "1";
    }

    std::string clock_port_name = mod->get_string_attribute("tmrx_clock_port_name");
    if (!clock_port_name.empty()) {
        cfg.clock_port_name = clock_port_name;
    }

    std::string rst_port_name = mod->get_string_attribute("tmrx_rst_port_name");
    if (!rst_port_name.empty()) {
        cfg.reset_port_name = rst_port_name;
    }

    std::string clock_port_expand_str = mod->get_string_attribute("tmrx_expand_clock");
    if (!clock_port_expand_str.empty()) {
        cfg.expand_clock = clock_port_expand_str == "1";
    }

    std::string rst_port_expand_str = mod->get_string_attribute("tmrx_expand_rst");
    if (!rst_port_expand_str.empty()) {
        cfg.expand_reset = rst_port_expand_str == "1";
    }

    std::string logic_path_1_suffix = mod->get_string_attribute("tmrx_logic_path_1_suffix");
    if (!logic_path_1_suffix.empty()) {
        cfg.logic_path_1_suffix = logic_path_1_suffix;
    }

    std::string logic_path_2_suffix = mod->get_string_attribute("tmrx_logic_path_2_suffix");
    if (!logic_path_1_suffix.empty()) {
        cfg.logic_path_2_suffix = logic_path_2_suffix;
    }

    std::string logic_path_3_suffix = mod->get_string_attribute("tmrx_logic_path_3_suffix");
    if (!logic_path_3_suffix.empty()) {
        cfg.logic_path_3_suffix = logic_path_3_suffix;
    }

    return cfg;
}

ConfigManager::ConfigManager(Yosys::RTLIL::Design *design, const std::string &cfg_file) {

    load_global_default_cfg();
    load_default_groups_cfg();

    toml::value t = toml::parse(cfg_file);
    if (t.is_empty()) {
        Yosys::log_warning(
            "No config file found, tmrx will proceed with default config and annotations");
        return;
    }

    global_cfg = parse_config(t.at("global"), global_cfg);

    for (auto &[table_name, table_value] : t.as_table()) {
        if (table_name == "global" && table_name == "module_groups") {
            continue;
        }

        if (table_name.rfind(cfg_group_prefix, 0) == 0) {
            group_cfg[table_name.substr(cfg_group_prefix.size())] =
                parse_config(table_value, global_cfg);
        }
    }

    Yosys::dict<Yosys::RTLIL::IdString, std::string> group_assigments;

    if (t.contains("module_groups")) {
        const auto &module_groups_table = toml::get<toml::value>(t.at("module_groups"));

        for (auto &[mod_name, group_value] : module_groups_table.as_table()) {
            std::string group_name = toml::get<std::string>(group_value);
            group_assigments[Yosys::RTLIL::escape_id(mod_name)] = group_name;
        }
    }

    for (auto module : design->modules()) {
        Yosys::RTLIL::IdString mod_name = module->name;

        Config def = global_cfg;

        std::string group_name = module->get_string_attribute("tmrx_assign_to_group");

        if (group_assigments.count(mod_name) > 0) {
            group_name = group_assigments[mod_name];
        }

        if (group_cfg.count(group_name) > 0) {
            def = group_cfg[group_name];
        } else {
            Yosys::log_warning("Group %s for module %s does not exist, proceeding with default",
                               group_name, mod_name);
        }

        if(module->has_memories() || module->has_processes()){
            def = group_cfg["black_box_module"];
            Yosys::log_warning("Module %s contains memories or processes, module will be treated as black box");
        }

        if(module->get_blackbox_attribute() || module->has_memories() || module->has_processes()){
            def = group_cfg["black_box_module"];
        }

        cell_cfgs[mod_name] = parse_module_annotations(module, def);
    }

    // TODO: add automatic top module detection

    for (auto &[table_name, table_value] : t.as_table()) {
        if (table_name == "global" && table_name == "module_groups") {
            continue;
        }

        if (table_name.rfind(cfg_module_prefix, 0) == 0) {
            Yosys::IdString mod_name =
                Yosys::RTLIL::escape_id(table_name.substr(cfg_group_prefix.size()));
            if (cell_cfgs.count(mod_name) == 0) {
                Yosys::log_warning("Module %s does not exist in design", mod_name);
                continue;
            }

            Config def = cell_cfgs[mod_name];

            cell_cfgs[mod_name] = parse_config(table_value, def);
        }
    }
}

const Config* ConfigManager::cfg(Yosys::RTLIL::Module *mod) {
    return &cell_cfgs[mod->name];
}



std::string ConfigManager::cfg_as_string(Yosys::RTLIL::Module *mod) {
    Config cfg = cell_cfgs[mod->name];
    std::string ret = "";

    std::string tmr_mode;
    if (cfg.tmr_mode == Config::TmrMode::None)
        tmr_mode = "None";
    else if (cfg.tmr_mode == Config::TmrMode::FullModuleTMR)
        tmr_mode = "FullModuleTMR";
    else if (cfg.tmr_mode == Config::TmrMode::LogicTMR)
        tmr_mode = "LogicTMR";

    ret += "TMR-Mode: " + tmr_mode + "\n";

    std::string tmr_voter;
    if (cfg.tmr_voter == Config::TmrVoter::Default)
        tmr_voter = "Default";

    ret += "TMR-Voter " + tmr_voter + "\n";



    return ret;
}
