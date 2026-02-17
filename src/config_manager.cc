#include "config_manager.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include <iterator>
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

std::string ConfigManager::get_string_atrr_value_or(const Yosys::RTLIL::Module *mod,const std::string& attr, const std::string &def){
    std::string ret = def;
    if(mod->has_attribute(attr)){
        ret = mod->get_string_attribute(attr);
    }
    return ret;
}
bool ConfigManager::get_bool_atrr_value_or(const Yosys::RTLIL::Module *mod,const std::string& attr, bool def){
    bool ret = def;
    if(mod->has_attribute(attr)){
        ret = (mod->get_string_attribute(attr)) == "1";
    }
    return  ret;
}
int ConfigManager::get_int_atrr_value_or(const Yosys::RTLIL::Module *mod,const std::string& attr, int def){
    int ret = def;
    if(mod->has_attribute(attr)){
        ret = std::stoi(mod->get_string_attribute(attr));
    }
    return ret;
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
        cfg.ff_cells.insert("\\" + (cell));
    }

    for (auto &cell : toml::find_or<std::vector<std::string>>(t, "additional_ff_cells", {})) {
        cfg.additional_ff_cells.insert("\\" + (cell));
    }

    for (auto &cell : toml::find_or<std::vector<std::string>>(t, "excludet_ff_cells", {})) {
        cfg.excludet_ff_cells.insert("\\" + (cell));
    }

    return cfg;
}

Config ConfigManager::parse_module_annotations(const Yosys::RTLIL::Module *mod,
                                               const Config &default_cfg) {
    Config cfg(default_cfg);

    // TODO: move strings to header and factor out
    std::string tmr_mode_str = get_string_atrr_value_or(mod, cfg_tmr_mode_attr_name, "");
    if (!tmr_mode_str.empty()) {
        if (tmr_mode_str == "None")
            cfg.tmr_mode = Config::TmrMode::None;
        else if (tmr_mode_str == "FullModuleTMR")
            cfg.tmr_mode = Config::TmrMode::FullModuleTMR;
        else if (tmr_mode_str == "LogicTMR")
            cfg.tmr_mode = Config::TmrMode::LogicTMR;
    }

    std::string tmr_voter_str = get_string_atrr_value_or(mod, cfg_tmr_voter_attr_name, "");
    if (!tmr_voter_str.empty()) {
        if (tmr_voter_str == "Default")
            cfg.tmr_voter = Config::TmrVoter::Default;
    }

    cfg.preserv_module_ports = get_bool_atrr_value_or(mod, cfg_tmr_preserve_module_ports_attr_name, default_cfg.preserv_module_ports);

    cfg.insert_voter_before_ff = get_bool_atrr_value_or(mod, cfg_insert_voter_before_ff_attr_name, default_cfg.insert_voter_before_ff);
    cfg.insert_voter_after_ff = get_bool_atrr_value_or(mod, cfg_insert_voter_after_ff_attr_name, default_cfg.insert_voter_after_ff);

    cfg.tmr_mode_full_module_insert_voter_before_modules = get_bool_atrr_value_or(mod, cfg_tmr_mode_full_module_insert_voter_before_modules_attr_name, default_cfg.tmr_mode_full_module_insert_voter_before_modules);
    cfg.tmr_mode_full_module_insert_voter_after_modules = get_bool_atrr_value_or(mod, cfg_tmr_mode_full_module_insert_voter_after_modules_attr_name, default_cfg.tmr_mode_full_module_insert_voter_after_modules);

    cfg.clock_port_name = get_string_atrr_value_or(mod, cfg_clock_port_name_attr_name, default_cfg.clock_port_name);
    cfg.reset_port_name = get_string_atrr_value_or(mod, cfg_rst_port_name_attr_name, default_cfg.reset_port_name);

    cfg.expand_clock = get_bool_atrr_value_or(mod, cfg_expand_clock_attr_name, default_cfg.expand_clock);
    cfg.expand_reset = get_bool_atrr_value_or(mod, cfg_expand_rst_attr_name, default_cfg.expand_reset);

    cfg.logic_path_1_suffix = get_string_atrr_value_or(mod, cfg_logic_path_1_suffix_attr_name, default_cfg.logic_path_1_suffix);
    cfg.logic_path_2_suffix = get_string_atrr_value_or(mod, cfg_logic_path_2_suffix_attr_name, default_cfg.logic_path_2_suffix);
    cfg.logic_path_3_suffix = get_string_atrr_value_or(mod, cfg_logic_path_3_suffix_attr_name, default_cfg.logic_path_3_suffix);
    return cfg;
}

ConfigManager::ConfigManager(Yosys::RTLIL::Design *design, const std::string &cfg_file) {
    Yosys::log_header(design, "stated arg parsing");
    load_global_default_cfg();
    load_default_groups_cfg();

    toml::value t;
    try {
        t = toml::parse(cfg_file);
    } catch (const std::exception &e) {
        Yosys::log_warning("Tmrx was unable to load cfg file, will proceed with default config and annotation '%s': %s. Using defaults.",
                           cfg_file.c_str(), e.what());
    }

    if (t.is_empty()) {
        Yosys::log_warning(
            "Config file empty, tmrx will proceed with default config and annotation");
    }

    Yosys::log_header(design, "Parsing global cfg");

    if(t.contains("global")){
        global_cfg = parse_config(t.at("global"), global_cfg);
    }

    Yosys::log_header(design, "Parsing group cfgs");

    for (auto &[table_name, table_value] : t.as_table()) {
        if (table_name == "global" || table_name == "module_groups") {
            continue;
        }

        if (table_name.rfind(cfg_group_prefix, 0) == 0) {
            group_cfg[table_name.substr(cfg_group_prefix.size())] =
                parse_config(table_value, global_cfg);
        }
    }
    Yosys::log_header(design, "Parsing group assigments");
    Yosys::dict<Yosys::RTLIL::IdString, std::string> group_assigments;

    if (t.contains("module_groups")) {
        const auto &module_groups_table = toml::get<toml::value>(t.at("module_groups"));

        for (auto &[mod_name, group_value] : module_groups_table.as_table()) {
            std::string group_name = toml::get<std::string>(group_value);
            group_assigments["\\" + mod_name] = group_name;
        }
    }

    Yosys::log_header(design, "Parsing module attrs cfg");
    for (auto module : design->modules()) {
        if(!(module->has_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE))){
            continue;
        }
        Yosys::RTLIL::IdString mod_name = module->name;
        // Yosys::log_header(design, "Looking at Module %s", mod_name);

        Config def = global_cfg;

        std::string group_name = get_string_atrr_value_or(module, cfg_group_assignment_attr_name ,"");

        if (group_assigments.count(mod_name) > 0) {
            group_name = group_assigments[mod_name];
        }

        if (group_cfg.count(group_name) > 0) {
            def = group_cfg[group_name];
        } else {
            if(!group_name.empty()){
            Yosys::log_warning("Group %s for module %s does not exist, proceeding with default",
                               group_name, mod_name);

            }
        }

        if(module->has_memories() || module->has_processes()){
            group_name = "black_box_module";
            def = group_cfg[group_name];
            Yosys::log_warning("Module %s contains memories or processes, module will be treated as black box", mod_name);
        }

        if(module->get_blackbox_attribute() || module->has_memories() || module->has_processes()){
            group_name = "black_box_module";
            def = group_cfg[group_name];
        }

        if(!group_name.empty()){
            group_assigments[mod_name] = group_name;
        }
        // Yosys::log_header(design, "creating id string");
        module_cfgs[mod_name] = parse_module_annotations(module, def);
    }

    // TODO: add automatic top module detection
    // TODO: prevent overwriting of black box
    Yosys::log_header(design, "Parsing module cfgs");
    for (auto &[table_name, table_value] : t.as_table()) {
        if (table_name == "global" || table_name == "module_groups") {
            continue;
        }

        if (table_name.rfind(cfg_module_prefix, 0) == 0) {
            Yosys::IdString mod_name = "\\" + (table_name.substr(cfg_module_prefix.size()));
            if (module_cfgs.count(mod_name) == 0) {
                Yosys::log_warning("Module %s does not exist in design", mod_name);
                continue;
            }

            Config def = module_cfgs[mod_name];

            module_cfgs[mod_name] = parse_config(table_value, def);
        }
    }
}

const Config* ConfigManager::cfg(Yosys::RTLIL::Module *mod) const{
    if(module_cfgs.count(mod->name) == 0){
        return &global_cfg;
    }

    return &module_cfgs.at(mod->name);
}



std::string ConfigManager::cfg_as_string(Yosys::RTLIL::Module *mod) const {
    const Config *mcfg = cfg(mod);
    std::string ret = "";

    std::string true_str = "true";
    std::string false_str = "false";

    std::string tmr_mode;
    if (mcfg->tmr_mode == Config::TmrMode::None)
        tmr_mode = "None";
    else if (mcfg->tmr_mode == Config::TmrMode::FullModuleTMR)
        tmr_mode = "FullModuleTMR";
    else if (mcfg->tmr_mode == Config::TmrMode::LogicTMR)
        tmr_mode = "LogicTMR";

    ret += "TMR-Mode: " + tmr_mode + "\n";

    std::string tmr_voter;
    if (mcfg->tmr_voter == Config::TmrVoter::Default)
        tmr_voter = "Default";

    ret += "TMR-Voter " + tmr_voter + "\n";

    ret += "Preserve Mod Ports " + (mcfg->preserv_module_ports ? true_str : false_str) + "\n";

    ret += "Insert Voter before ff " + (mcfg->insert_voter_before_ff ? true_str : false_str) + "\n";

    ret += "Insert Voter after ff " + (mcfg->insert_voter_after_ff ? true_str : false_str) + "\n";

    ret += "Tmr Mode full; insert voter before module " + (mcfg->tmr_mode_full_module_insert_voter_before_modules ? true_str : false_str) + "\n";
    ret += "Tmr Mode full; insert voter after module " + (mcfg->tmr_mode_full_module_insert_voter_after_modules ? true_str : false_str) + "\n";

    ret += "Clock port name: " + mcfg->clock_port_name + "\n";
    ret += "Expand Clock net: " + (mcfg->expand_clock ? true_str : false_str) + "\n";

    ret += "Reset port name: " + mcfg->reset_port_name + "\n";
    ret += "Expand reset net: " + (mcfg->expand_reset ? true_str : false_str) + "\n";

    ret += "FF Cells: [";
    for (auto cell : mcfg->ff_cells){
        ret += cell.str() + ", ";
    }
    ret += "]\n";

    ret += "Additional FF Cells: [";
    for (auto cell : mcfg->additional_ff_cells){
        ret += cell.str() + ", ";
    }
    ret += "]\n";

    ret += "Excldet FF Cells: [";
    for (auto cell : mcfg->excludet_ff_cells){
        ret += cell.str() + ", ";
    }
    ret += "]\n";


    ret += "Logic path 1 suffix: " + mcfg->logic_path_1_suffix + "\n";
    ret += "Logic path 2 suffix: " + mcfg->logic_path_2_suffix + "\n";
    ret += "Logic path 3 suffix: " + mcfg->logic_path_3_suffix + "\n";


    return ret;
}
