#include "config_manager.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys_common.h"
#include <optional>
#include <string>
#include <vector>

void ConfigManager::load_global_default_cfg() {
    global_cfg = {};

    global_cfg.tmr_mode = TmrMode::LogicTMR;
    global_cfg.tmr_voter = TmrVoter::Default;

    global_cfg.preserve_module_ports = false;

    global_cfg.insert_voter_before_ff = false;
    global_cfg.insert_voter_after_ff = true;

    global_cfg.tmr_mode_full_module_insert_voter_before_modules = false;
    global_cfg.tmr_mode_full_module_insert_voter_after_modules = true;

    global_cfg.clock_port_names = {Yosys::RTLIL::IdString("\\clk_i")};
    global_cfg.reset_port_names = {Yosys::RTLIL::IdString("\\rst_ni")};

    global_cfg.expand_clock = false;
    global_cfg.expand_reset = false;

    global_cfg.ff_cells = {};
    global_cfg.additional_ff_cells = {};
    global_cfg.excluded_ff_cells = {};

    global_cfg.logic_path_1_suffix = "_a";
    global_cfg.logic_path_2_suffix = "_b";
    global_cfg.logic_path_3_suffix = "_c";
}

void ConfigManager::load_default_groups_cfg() {
    // TODO: parmatrize
    group_cfg["black_box_module"] = ConfigPart();
    group_cfg["black_box_module"].tmr_mode = TmrMode::FullModuleTMR;

    group_cfg["cdc_module"] = ConfigPart();
    group_cfg["cdc_module"].tmr_mode = TmrMode::FullModuleTMR;

    // TODO: add top module
}

std::string ConfigManager::get_string_attr_value_or(const Yosys::RTLIL::Module *mod,
                                                    const std::string &attr,
                                                    const std::string &def) {
    std::string ret = def;
    if (mod->has_attribute(attr)) {
        ret = mod->get_string_attribute(attr);
    }
    return ret;
}
bool ConfigManager::get_bool_attr_value_or(const Yosys::RTLIL::Module *mod, const std::string &attr,
                                           bool def) {
    bool ret = def;
    if (mod->has_attribute(attr)) {
        ret = (mod->get_string_attribute(attr)) == "1";
    }
    return ret;
}
int ConfigManager::get_int_attr_value_or(const Yosys::RTLIL::Module *mod, const std::string &attr,
                                         int def) {
    int ret = def;
    if (mod->has_attribute(attr)) {
        ret = std::stoi(mod->get_string_attribute(attr));
    }
    return ret;
}


std::optional<std::string> ConfigManager::get_string_attr_value(const Yosys::RTLIL::Module *mod,
                                                    const std::string &attr) {
    if (mod->has_attribute(attr)) {
        return  mod->get_string_attribute(attr);
    }
    return std::nullopt;
}
std::optional<bool> ConfigManager::get_bool_attr_value(const Yosys::RTLIL::Module *mod, const std::string &attr) {
    if (mod->has_attribute(attr)) {
        return  (mod->get_string_attribute(attr)) == "1";
    }
    return std::nullopt;
}
std::optional<int> ConfigManager::get_int_attr_value(const Yosys::RTLIL::Module *mod, const std::string &attr) {
    if (mod->has_attribute(attr)) {
        return std::stoi(mod->get_string_attribute(attr));
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>> ConfigManager::get_string_list_attr_value(const Yosys::RTLIL::Module *mod,
                                                    const std::string &attr) {
    if (mod->has_attribute(attr)) {
        std::vector<std::string> res;
        std::string attr_list = mod->get_string_attribute(attr);
        std::stringstream ss(attr_list);
        std::string attri;

        while (std::getline(ss, attri, ';')) {
            res.push_back(attri);
        }
        return res;
    }
    return std::nullopt;
}

ConfigPart ConfigManager::parse_config(const toml::value &t) {
    ConfigPart cfg;

    std::string tmr_mode_str = toml::find_or<std::string>(t, "tmr_mode", "");

    // TODO: enum str parserser <- ->

    if (!tmr_mode_str.empty()) {
        if (tmr_mode_str == "None")
            cfg.tmr_mode = TmrMode::None;
        else if (tmr_mode_str == "FullModuleTMR")
            cfg.tmr_mode = TmrMode::FullModuleTMR;
        else if (tmr_mode_str == "LogicTMR")
            cfg.tmr_mode = TmrMode::LogicTMR;
    }

    std::string tmr_voter_str = toml::find_or<std::string>(t, "tmr_voter", "");
    if (!tmr_voter_str.empty()) {
        if (tmr_voter_str == "Default")
            cfg.tmr_voter = TmrVoter::Default;
    }

    if(t.contains("preserve_module_ports")){
        cfg.preserve_module_ports = toml::find<bool>(t, "preserve_module_ports");
    }


    if(t.contains("insert_voter_before_ff")){
        cfg.insert_voter_before_ff = toml::find<bool>(t, "insert_voter_before_ff");
    }

    if(t.contains("insert_voter_after_ff")){
    cfg.insert_voter_after_ff =
        toml::find<bool>(t, "insert_voter_after_ff");
    }

    if(t.contains("clock_port_names")){
        auto clk_ports = toml::find_or<std::vector<std::string>>(t, "clock_port_names", {});
        if(!cfg.clock_port_names){
            cfg.clock_port_names.emplace();
        }

        for (auto &port : clk_ports) {
            cfg.clock_port_names->insert(Yosys::RTLIL::IdString("\\" + port));
        }
    }

    if(t.contains("expand_clock")){
        cfg.expand_clock = toml::find<bool>(t, "expand_clock");
    }

    if(t.contains("reset_port_names")){
        auto rst_ports = toml::find_or<std::vector<std::string>>(t, "reset_port_names", {});
        if(!cfg.reset_port_names){
            cfg.reset_port_names.emplace();
        }
        for (auto &port : rst_ports) {
            cfg.reset_port_names->insert(Yosys::RTLIL::IdString("\\" + port));
        }
    }

    if(t.contains("expand_reset")){
        cfg.expand_reset = toml::find<bool>(t, "expand_reset");
    }

    if(t.contains("tmr_mode_full_module_insert_voter_after_modules")){
    cfg.tmr_mode_full_module_insert_voter_after_modules =
        toml::find<bool>(t, "tmr_mode_full_module_insert_voter_after_modules");

    }

    if(t.contains("tmr_mode_full_module_insert_voter_before_modules")){
    cfg.tmr_mode_full_module_insert_voter_before_modules =
        toml::find<bool>(t, "tmr_mode_full_module_insert_voter_before_modules");
    }

    if(t.contains("logic_path_1_suffix")){
        cfg.logic_path_1_suffix = toml::find<std::string>(t, "logic_path_1_suffix");
    }

    if(t.contains("logic_path_2_suffix")){
        cfg.logic_path_2_suffix = toml::find<std::string>(t, "logic_path_2_suffix");
    }

    if(t.contains("logic_path_3_suffix")){
        cfg.logic_path_3_suffix = toml::find<std::string>(t, "logic_path_3_suffix");
    }

    if(t.contains("ff_cells")){
        auto ff_cells = toml::find_or<std::vector<std::string>>(t, "ff_cells", {});
        if(!cfg.ff_cells){
            cfg.ff_cells.emplace();
        }

        for (auto &cell : ff_cells) {
            cfg.ff_cells->insert("\\" + (cell));
        }
    }


    if(t.contains("additional_ff_cells")){
        auto additional_ffs = toml::find_or<std::vector<std::string>>(t, "additional_ff_cells", {});
        if(!cfg.additional_ff_cells){
            cfg.additional_ff_cells.emplace();
        }

        for (auto &cell : additional_ffs) {
            cfg.additional_ff_cells->insert("\\" + (cell));
        }
    }

    if(t.contains("excluded_ff_cells")){
        auto excl_ffs = toml::find_or<std::vector<std::string>>(t, "excluded_ff_cells", {});
        if(!cfg.excluded_ff_cells){
            cfg.excluded_ff_cells.emplace();
        }

        for (auto &cell : excl_ffs) {
            cfg.excluded_ff_cells->insert("\\" + (cell));
        }
    }

    return cfg;
}

ConfigPart ConfigManager::parse_module_annotations(const Yosys::RTLIL::Module *mod) {
    ConfigPart cfg;

    // TODO: move strings to header and factor out
    std::string tmr_mode_str = get_string_attr_value_or(mod, cfg_tmr_mode_attr_name, "");
    if (!tmr_mode_str.empty()) {
        if (tmr_mode_str == "None")
            cfg.tmr_mode = TmrMode::None;
        else if (tmr_mode_str == "FullModuleTMR")
            cfg.tmr_mode = TmrMode::FullModuleTMR;
        else if (tmr_mode_str == "LogicTMR")
            cfg.tmr_mode = TmrMode::LogicTMR;
    }

    std::string tmr_voter_str = get_string_attr_value_or(mod, cfg_tmr_voter_attr_name, "");
    if (!tmr_voter_str.empty()) {
        if (tmr_voter_str == "Default")
            cfg.tmr_voter = TmrVoter::Default;
    }

    cfg.preserve_module_ports = get_bool_attr_value(mod, cfg_tmr_preserve_module_ports_attr_name);

    cfg.insert_voter_before_ff = get_bool_attr_value(mod, cfg_insert_voter_before_ff_attr_name);
    cfg.insert_voter_after_ff = get_bool_attr_value(mod, cfg_insert_voter_after_ff_attr_name);

    cfg.tmr_mode_full_module_insert_voter_before_modules =
        get_bool_attr_value(mod, cfg_tmr_mode_full_module_insert_voter_before_modules_attr_name);
    cfg.tmr_mode_full_module_insert_voter_after_modules =
        get_bool_attr_value(mod, cfg_tmr_mode_full_module_insert_voter_after_modules_attr_name);

    auto clk_port_strs = get_string_list_attr_value(mod, cfg_clock_port_name_attr_name);
    if(clk_port_strs){
        if (!cfg.clock_port_names){
            cfg.clock_port_names.emplace();
        }
        for(auto &port_name : *clk_port_strs){
            cfg.clock_port_names->insert(Yosys::RTLIL::IdString("\\" + port_name));
        }
    }

    auto rst_port_strs = get_string_list_attr_value(mod, cfg_rst_port_name_attr_name);
    if(rst_port_strs){
        if(!cfg.reset_port_names){
            cfg.reset_port_names.emplace();
        }

        for(auto &port_name : *rst_port_strs){
            cfg.reset_port_names->insert(Yosys::RTLIL::IdString("\\" + port_name));
        }
    }

    cfg.expand_clock =
        get_bool_attr_value(mod, cfg_expand_clock_attr_name);
    cfg.expand_reset =
        get_bool_attr_value(mod, cfg_expand_rst_attr_name);

    cfg.logic_path_1_suffix = get_string_attr_value(mod, cfg_logic_path_1_suffix_attr_name);
    cfg.logic_path_2_suffix = get_string_attr_value(mod, cfg_logic_path_2_suffix_attr_name);
    cfg.logic_path_3_suffix = get_string_attr_value(mod, cfg_logic_path_3_suffix_attr_name);
    return cfg;
}

Config ConfigManager::assemble_config(std::vector<ConfigPart> parts, Config def){
    Config cfg(def);
    for(auto &cfg_part : parts){
        if(cfg_part.tmr_mode){
            cfg.tmr_mode = *cfg_part.tmr_mode;
        }
        if(cfg_part.tmr_voter){
            cfg.tmr_voter = *cfg_part.tmr_voter;
        }
        if(cfg_part.preserve_module_ports){
            cfg.preserve_module_ports = *cfg_part.preserve_module_ports;
        }
        if(cfg_part.insert_voter_before_ff){
            cfg.insert_voter_before_ff = *cfg_part.insert_voter_before_ff;
        }
        if(cfg_part.insert_voter_after_ff){
            cfg.insert_voter_after_ff = *cfg_part.insert_voter_after_ff;
        }
        if(cfg_part.tmr_mode_full_module_insert_voter_before_modules){
            cfg.tmr_mode_full_module_insert_voter_before_modules = *cfg_part.tmr_mode_full_module_insert_voter_before_modules;
        }
        if(cfg_part.tmr_mode_full_module_insert_voter_after_modules){
            cfg.tmr_mode_full_module_insert_voter_after_modules = *cfg_part.tmr_mode_full_module_insert_voter_after_modules;
        }
        if(cfg_part.clock_port_names){
            cfg.clock_port_names = *cfg_part.clock_port_names;
        }
        if(cfg_part.expand_clock){
            cfg.expand_clock = *cfg_part.expand_clock;
        }
        if(cfg_part.reset_port_names){
            cfg.reset_port_names = *cfg_part.reset_port_names;
        }
        if(cfg_part.expand_reset){
            cfg.expand_reset = *cfg_part.expand_reset;
        }
        if(cfg_part.ff_cells){
            cfg.ff_cells = *cfg_part.ff_cells;
        }
        if(cfg_part.additional_ff_cells){
            cfg.additional_ff_cells = *cfg_part.additional_ff_cells;
        }
        if(cfg_part.excluded_ff_cells){
            cfg.excluded_ff_cells = *cfg_part.excluded_ff_cells;
        }
        if(cfg_part.logic_path_1_suffix){
            cfg.logic_path_1_suffix = *cfg_part.logic_path_1_suffix;
        }
        if(cfg_part.logic_path_2_suffix){
            cfg.logic_path_2_suffix = *cfg_part.logic_path_2_suffix;
        }
        if(cfg_part.logic_path_3_suffix){
            cfg.logic_path_3_suffix = *cfg_part.logic_path_3_suffix;
        }
    }
    return cfg;
}

ConfigManager::ConfigManager(Yosys::RTLIL::Design *design, const std::string &cfg_file) {
    Yosys::log_header(design, "stated arg parsing");
    load_global_default_cfg();
    load_default_groups_cfg();

    bool loaded_cfg = true;

    toml::value t;
    try {
        t = toml::parse(cfg_file);
    } catch (const std::exception &e) {
        Yosys::log_warning("Tmrx was unable to load cfg file, will proceed with default config and "
                           "annotation '%s': %s. Using defaults.",
                           cfg_file.c_str(), e.what());
        loaded_cfg = false;
    }

    if (t.is_empty()) {
        Yosys::log_warning(
            "Config file empty, tmrx will proceed with default config and annotation");
    }

    Yosys::log_header(design, "Parsing global cfg");
    ConfigPart global_config_part;
    if (loaded_cfg && t.contains("global")) {
        global_config_part = parse_config(t.at("global"));
        global_cfg = assemble_config({global_config_part}, global_cfg);
    }

    Yosys::log_header(design, "Parsing group cfgs");
    if (loaded_cfg) {
        for (auto &[table_name, table_value] : t.as_table()) {
            if (table_name == "global" || table_name == "module_groups") {
                continue;
            }

            if (table_name.rfind(cfg_group_prefix, 0) == 0) {
                group_cfg[table_name.substr(cfg_group_prefix.size())] =
                    parse_config(table_value);
            }
        }
    }



    Yosys::log_header(design, "Parsing group assignments");
    Yosys::dict<Yosys::RTLIL::IdString, std::vector<std::string>> group_assignments;

    if (loaded_cfg && t.contains("module_groups")) {
        const auto &module_groups_table = toml::get<toml::value>(t.at("module_groups"));

        for (auto &[mod_name, group_value] : module_groups_table.as_table()) {
            std::string group_name = toml::get<std::string>(group_value);
            group_assignments["\\" + mod_name].push_back(group_name);
        }
    }

    Yosys::log_header(design, "Parsing module attrs cfg");
    for (auto module : design->modules()) {
        if (!(module->has_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE))) {
            continue;
        }
        Yosys::RTLIL::IdString mod_name = module->name;


        auto group_names = get_string_list_attr_value(module, cfg_group_assignment_attr_name);

        if (group_names) {
            for(const auto &name : *group_names){
                group_assignments[mod_name].push_back(name);
            }
        }


        if (module->has_memories() || module->has_processes()) {

            group_assignments[mod_name].push_back("black_box_module");
            Yosys::log_warning(
                "Module %s contains memories or processes, module will be treated as black box",
                mod_name.c_str());
        }

        if (module->get_blackbox_attribute()) {
            group_assignments[mod_name].push_back("black_box_module");
        }

        // Yosys::log_header(design, "creating id string");
        module_attr_cfgs[mod_name] = parse_module_annotations(module);
    }

    // TODO: add automatic top module detection
    // TODO: prevent overwriting of black box
    Yosys::log_header(design, "Parsing specific module cfgs");
    if (loaded_cfg) {
        for (auto &[table_name, table_value] : t.as_table()) {
            if (table_name == "global" || table_name == "module_groups") {
                continue;
            }


            if (table_name.rfind(cfg_specific_module_prefix, 0) == 0) {
                if (table_name.find('$') == std::string::npos) {
                    Yosys::log_error("Specific module configurations are for readslangs auto uniquified module names, use module config\n");
                }

                specific_module_cfgs["\\" + table_name.substr(cfg_specific_module_prefix.size())] =
                    parse_config(table_value);
            }
        }
    }

    Yosys::log_header(design, "Parsing module cfgs");
    if (loaded_cfg) {
        for (auto &[table_name, table_value] : t.as_table()) {
            if (table_name == "global" || table_name == "module_groups") {
                continue;
            }


            if ((table_name.rfind(cfg_module_prefix, 0) == 0) && !(table_name.rfind(cfg_specific_module_prefix, 0) == 0)){

                module_cfgs["\\" + table_name.substr(cfg_module_prefix.size())] =
                    parse_config(table_value);
            }
        }
    }

    Yosys::log_header(design, "Assembling final module cfgs");
    for (auto module : design->modules()) {
        if (!(module->has_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE))) {
            continue;
        }

        Yosys::RTLIL::IdString specific_mod_name = module->name;
        Yosys::RTLIL::IdString mod_name = specific_mod_name.str().substr(0, specific_mod_name.str().find('$'));

        Yosys::log("Assembling for %s| %s\n", specific_mod_name.c_str(), mod_name.c_str());

        std::vector<ConfigPart> cfg_parts = {global_config_part};

        if(group_assignments.count(mod_name) != 0){
            for(auto &asig : group_assignments.at(mod_name)){
                if(group_cfg.count(asig) == 0){
                    Yosys::log_warning("Warning group %s for module %s | %s not found, skipping assignment", asig.c_str(), specific_mod_name.c_str(), mod_name.c_str());
                    continue;
                }
                cfg_parts.push_back(group_cfg.at(asig));
            }
        }

        if(group_assignments.count(specific_mod_name) != 0){
            for(auto &asig : group_assignments.at(specific_mod_name)){
                if(group_cfg.count(asig) == 0){
                    Yosys::log_warning("Warning group %s for module %s | %s not found, skipping assignment", asig.c_str(), specific_mod_name.c_str(), mod_name.c_str());
                    continue;
                }

                cfg_parts.push_back(group_cfg.at(asig));
            }
        }

        if(module_cfgs.count(mod_name) != 0){
            cfg_parts.push_back(module_cfgs.at(mod_name));
        }

        if(module_attr_cfgs.count(specific_mod_name) != 0){
            cfg_parts.push_back(module_attr_cfgs.at(specific_mod_name));
        }

        if(specific_module_cfgs.count(specific_mod_name) != 0 ){
            cfg_parts.push_back(specific_module_cfgs.at(specific_mod_name));
        }

        // TODO: validate cfg and enforce some values
        final_module_cfgs[specific_mod_name] = assemble_config(cfg_parts, global_cfg);
    }


}

const Config *ConfigManager::cfg(Yosys::RTLIL::Module *mod) const {
    if (final_module_cfgs.count(mod->name) == 0) {
        return &global_cfg;
    }

    return &final_module_cfgs.at(mod->name);
}

std::string ConfigManager::cfg_as_string(Yosys::RTLIL::Module *mod) const {
    const Config *mcfg = cfg(mod);
    std::string ret = "";

    std::string true_str = "true";
    std::string false_str = "false";

    std::string tmr_mode;
    if (mcfg->tmr_mode == TmrMode::None)
        tmr_mode = "None";
    else if (mcfg->tmr_mode == TmrMode::FullModuleTMR)
        tmr_mode = "FullModuleTMR";
    else if (mcfg->tmr_mode == TmrMode::LogicTMR)
        tmr_mode = "LogicTMR";

    ret += "TMR-Mode: " + tmr_mode + "\n";

    std::string tmr_voter;
    if (mcfg->tmr_voter == TmrVoter::Default)
        tmr_voter = "Default";

    ret += "TMR-Voter " + tmr_voter + "\n";

    ret += "Preserve Mod Ports " + (mcfg->preserve_module_ports ? true_str : false_str) + "\n";

    ret += "Insert Voter before ff " + (mcfg->insert_voter_before_ff ? true_str : false_str) + "\n";

    ret += "Insert Voter after ff " + (mcfg->insert_voter_after_ff ? true_str : false_str) + "\n";

    ret += "Tmr Mode full; insert voter before module " +
           (mcfg->tmr_mode_full_module_insert_voter_before_modules ? true_str : false_str) + "\n";
    ret += "Tmr Mode full; insert voter after module " +
           (mcfg->tmr_mode_full_module_insert_voter_after_modules ? true_str : false_str) + "\n";

    ret += "Clock port names: [";
    for(auto clp : mcfg->clock_port_names){
        ret += clp.str() + ", ";
    }
    ret += "]\n";

    ret += "Expand Clock net: " + (mcfg->expand_clock ? true_str : false_str) + "\n";

    ret += "Reset port names: [";
    for(auto rp : mcfg->reset_port_names){
        ret += rp.str() + ", ";
    }
    ret += "]\n";

    ret += "Expand reset net: " + (mcfg->expand_reset ? true_str : false_str) + "\n";

    ret += "FF Cells: [";
    for (auto cell : mcfg->ff_cells) {
        ret += cell.str() + ", ";
    }
    ret += "]\n";

    ret += "Additional FF Cells: [";
    for (auto cell : mcfg->additional_ff_cells) {
        ret += cell.str() + ", ";
    }
    ret += "]\n";

    ret += "Excluded FF Cells: [";
    for (auto cell : mcfg->excluded_ff_cells) {
        ret += cell.str() + ", ";
    }
    ret += "]\n";

    ret += "Logic path 1 suffix: " + mcfg->logic_path_1_suffix + "\n";
    ret += "Logic path 2 suffix: " + mcfg->logic_path_2_suffix + "\n";
    ret += "Logic path 3 suffix: " + mcfg->logic_path_3_suffix + "\n";

    return ret;
}
