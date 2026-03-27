#include "config_manager.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include <optional>
#include <string>
#include <vector>

// ============================================================================
// String conversion helpers
// ============================================================================

std::optional<TmrMode> parse_tmr_mode(const std::string &str) {
    if (str.empty()) return std::nullopt;
    if (str == "None") return TmrMode::None;
    if (str == "FullModuleTMR") return TmrMode::FullModuleTMR;
    if (str == "LogicTMR") return TmrMode::LogicTMR;
    return std::nullopt;
}

std::string tmr_mode_to_string(TmrMode mode) {
    switch (mode) {
        case TmrMode::None: return "None";
        case TmrMode::FullModuleTMR: return "FullModuleTMR";
        case TmrMode::LogicTMR: return "LogicTMR";
    }
    return "Unknown";
}

std::optional<TmrVoter> parse_tmr_voter(const std::string &str) {
    if (str.empty()) return std::nullopt;
    if (str == "Default") return TmrVoter::Default;
    if (str == "Custom")  return TmrVoter::Custom;
    return std::nullopt;
}

std::string tmr_voter_to_string(TmrVoter voter) {
    switch (voter) {
        case TmrVoter::Default: return "Default";
        case TmrVoter::Custom:  return "Custom";
    }
    return "Unknown";
}

// ============================================================================
// TOML parsing helpers
// ============================================================================

template<typename T>
std::optional<T> toml_find_optional(const toml::value &t, const std::string &key) {
    if (t.contains(key)) {
        return toml::find<T>(t, key);
    }
    return std::nullopt;
}

// Explicit template instantiations
template std::optional<bool> toml_find_optional<bool>(const toml::value &t, const std::string &key);
template std::optional<std::string> toml_find_optional<std::string>(const toml::value &t, const std::string &key);

std::optional<Yosys::pool<Yosys::RTLIL::IdString>>
toml_parse_idstring_pool(const toml::value &t, const std::string &key) {
    if (!t.contains(key)) return std::nullopt;

    auto items = toml::find_or<std::vector<std::string>>(t, key, {});
    Yosys::pool<Yosys::RTLIL::IdString> pool;
    for (const auto &item : items) {
        pool.insert(Yosys::RTLIL::IdString("\\" + item));
    }
    return pool;
}

// ============================================================================
// Config assembly helper
// ============================================================================

template<typename T>
void apply_if_present(T &dest, const std::optional<T> &src) {
    if (src) {
        dest = *src;
    }
}

// Explicit template instantiations for common types
template void apply_if_present<TmrMode>(TmrMode &dest, const std::optional<TmrMode> &src);
template void apply_if_present<TmrVoter>(TmrVoter &dest, const std::optional<TmrVoter> &src);
template void apply_if_present<bool>(bool &dest, const std::optional<bool> &src);
template void apply_if_present<std::string>(std::string &dest, const std::optional<std::string> &src);
template void apply_if_present<Yosys::pool<Yosys::RTLIL::IdString>>(
    Yosys::pool<Yosys::RTLIL::IdString> &dest,
    const std::optional<Yosys::pool<Yosys::RTLIL::IdString>> &src);

// ============================================================================
// String formatting helpers
// ============================================================================

std::string pool_to_string(const Yosys::pool<Yosys::RTLIL::IdString> &pool) {
    std::string ret = "[";
    for (const auto &item : pool) {
        ret += item.str() + ", ";
    }
    ret += "]";
    return ret;
}

std::string bool_to_string(bool val) {
    return val ? "true" : "false";
}

// ============================================================================
// ConfigManager implementation
// ============================================================================

void ConfigManager::load_global_default_cfg() {
    global_cfg = {};

    global_cfg.tmr_mode = TmrMode::LogicTMR;
    global_cfg.tmr_voter = TmrVoter::Default;
    global_cfg.tmr_voter_file = "";
    global_cfg.tmr_voter_module = "";
    global_cfg.tmr_voter_clock_port_name = "";
    global_cfg.tmr_voter_reset_port_name = "";
    global_cfg.tmr_voter_clock_net = "";
    global_cfg.tmr_voter_reset_net = "";
    global_cfg.tmr_voter_safe_mode = true;

    global_cfg.preserve_module_ports = false;

    global_cfg.insert_voter_before_ff = false;
    global_cfg.insert_voter_after_ff = true;

    global_cfg.tmr_mode_full_module_insert_voter_before_modules = false;
    global_cfg.tmr_mode_full_module_insert_voter_after_modules = true;
    global_cfg.tmr_mode_full_module_insert_voter_on_clock_nets = false;
    global_cfg.tmr_mode_full_module_insert_voter_on_reset_nets = false;

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

    global_cfg.error_port_name = "";
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

    // Parse enum fields
    cfg.tmr_mode = parse_tmr_mode(toml::find_or<std::string>(t, "tmr_mode", ""));
    cfg.tmr_voter = parse_tmr_voter(toml::find_or<std::string>(t, "tmr_voter", ""));

    // Parse boolean fields
    cfg.tmr_voter_safe_mode = toml_find_optional<bool>(t, "tmr_voter_safe_mode");
    cfg.preserve_module_ports = toml_find_optional<bool>(t, "preserve_module_ports");
    cfg.insert_voter_before_ff = toml_find_optional<bool>(t, "insert_voter_before_ff");
    cfg.insert_voter_after_ff = toml_find_optional<bool>(t, "insert_voter_after_ff");
    cfg.tmr_mode_full_module_insert_voter_before_modules =
        toml_find_optional<bool>(t, "tmr_mode_full_module_insert_voter_before_modules");
    cfg.tmr_mode_full_module_insert_voter_after_modules =
        toml_find_optional<bool>(t, "tmr_mode_full_module_insert_voter_after_modules");
    cfg.tmr_mode_full_module_insert_voter_on_clock_nets =
        toml_find_optional<bool>(t, "tmr_mode_full_module_insert_voter_on_clock_nets");
    cfg.tmr_mode_full_module_insert_voter_on_reset_nets =
        toml_find_optional<bool>(t, "tmr_mode_full_module_insert_voter_on_reset_nets");
    cfg.expand_clock = toml_find_optional<bool>(t, "expand_clock");
    cfg.expand_reset = toml_find_optional<bool>(t, "expand_reset");

    // Parse string fields
    cfg.tmr_voter_file             = toml_find_optional<std::string>(t, "tmr_voter_file");
    cfg.tmr_voter_module           = toml_find_optional<std::string>(t, "tmr_voter_module");
    cfg.tmr_voter_clock_port_name  = toml_find_optional<std::string>(t, "tmr_voter_clock_port_name");
    cfg.tmr_voter_reset_port_name  = toml_find_optional<std::string>(t, "tmr_voter_reset_port_name");
    cfg.tmr_voter_clock_net        = toml_find_optional<std::string>(t, "tmr_voter_clock_net");
    cfg.tmr_voter_reset_net        = toml_find_optional<std::string>(t, "tmr_voter_reset_net");
    cfg.logic_path_1_suffix = toml_find_optional<std::string>(t, "logic_path_1_suffix");
    cfg.logic_path_2_suffix = toml_find_optional<std::string>(t, "logic_path_2_suffix");
    cfg.logic_path_3_suffix = toml_find_optional<std::string>(t, "logic_path_3_suffix");
    cfg.error_port_name     = toml_find_optional<std::string>(t, "error_port_name");

    // Parse IdString pool fields
    cfg.clock_port_names = toml_parse_idstring_pool(t, "clock_port_names");
    cfg.reset_port_names = toml_parse_idstring_pool(t, "reset_port_names");
    cfg.ff_cells = toml_parse_idstring_pool(t, "ff_cells");
    cfg.additional_ff_cells = toml_parse_idstring_pool(t, "additional_ff_cells");
    cfg.excluded_ff_cells = toml_parse_idstring_pool(t, "excluded_ff_cells");

    return cfg;
}

std::optional<Yosys::pool<Yosys::RTLIL::IdString>>
ConfigManager::parse_attr_idstring_pool(const Yosys::RTLIL::Module *mod, const std::string &attr) {
    auto strs = get_string_list_attr_value(mod, attr);
    if (!strs) return std::nullopt;

    Yosys::pool<Yosys::RTLIL::IdString> pool;
    for (const auto &s : *strs) {
        pool.insert(Yosys::RTLIL::IdString("\\" + s));
    }
    return pool;
}

ConfigPart ConfigManager::parse_module_annotations(const Yosys::RTLIL::Module *mod) {
    ConfigPart cfg;

    // Parse enum fields
    cfg.tmr_mode = parse_tmr_mode(get_string_attr_value_or(mod, cfg_tmr_mode_attr_name, ""));
    cfg.tmr_voter = parse_tmr_voter(get_string_attr_value_or(mod, cfg_tmr_voter_attr_name, ""));

    // Parse boolean fields
    cfg.tmr_voter_safe_mode = get_bool_attr_value(mod, cfg_tmr_voter_safe_mode_attr_name);
    cfg.preserve_module_ports = get_bool_attr_value(mod, cfg_tmr_preserve_module_ports_attr_name);
    cfg.insert_voter_before_ff = get_bool_attr_value(mod, cfg_insert_voter_before_ff_attr_name);
    cfg.insert_voter_after_ff = get_bool_attr_value(mod, cfg_insert_voter_after_ff_attr_name);
    cfg.tmr_mode_full_module_insert_voter_before_modules =
        get_bool_attr_value(mod, cfg_tmr_mode_full_module_insert_voter_before_modules_attr_name);
    cfg.tmr_mode_full_module_insert_voter_after_modules =
        get_bool_attr_value(mod, cfg_tmr_mode_full_module_insert_voter_after_modules_attr_name);
    cfg.tmr_mode_full_module_insert_voter_on_clock_nets =
        get_bool_attr_value(mod, cfg_tmr_mode_full_module_insert_voter_on_clock_nets_attr_name);
    cfg.tmr_mode_full_module_insert_voter_on_reset_nets =
        get_bool_attr_value(mod, cfg_tmr_mode_full_module_insert_voter_on_reset_nets_attr_name);
    cfg.expand_clock = get_bool_attr_value(mod, cfg_expand_clock_attr_name);
    cfg.expand_reset = get_bool_attr_value(mod, cfg_expand_rst_attr_name);

    // Parse string fields
    cfg.tmr_voter_file            = get_string_attr_value(mod, cfg_tmr_voter_file_attr_name);
    cfg.tmr_voter_module          = get_string_attr_value(mod, cfg_tmr_voter_module_attr_name);
    cfg.tmr_voter_clock_port_name = get_string_attr_value(mod, cfg_tmr_voter_clock_port_name_attr_name);
    cfg.tmr_voter_reset_port_name = get_string_attr_value(mod, cfg_tmr_voter_reset_port_name_attr_name);
    cfg.tmr_voter_clock_net       = get_string_attr_value(mod, cfg_tmr_voter_clock_net_attr_name);
    cfg.tmr_voter_reset_net       = get_string_attr_value(mod, cfg_tmr_voter_reset_net_attr_name);
    cfg.logic_path_1_suffix = get_string_attr_value(mod, cfg_logic_path_1_suffix_attr_name);
    cfg.logic_path_2_suffix = get_string_attr_value(mod, cfg_logic_path_2_suffix_attr_name);
    cfg.logic_path_3_suffix = get_string_attr_value(mod, cfg_logic_path_3_suffix_attr_name);
    cfg.error_port_name     = get_string_attr_value(mod, cfg_error_port_name_attr_name);

    // Parse IdString pool fields
    cfg.clock_port_names = parse_attr_idstring_pool(mod, cfg_clock_port_name_attr_name);
    cfg.reset_port_names = parse_attr_idstring_pool(mod, cfg_rst_port_name_attr_name);

    return cfg;
}

Config ConfigManager::assemble_config(std::vector<ConfigPart> parts, Config def){
    Config cfg(def);
    for (const auto &part : parts) {
        apply_if_present(cfg.tmr_mode, part.tmr_mode);
        apply_if_present(cfg.tmr_voter, part.tmr_voter);
        apply_if_present(cfg.tmr_voter_file, part.tmr_voter_file);
        apply_if_present(cfg.tmr_voter_module, part.tmr_voter_module);
        apply_if_present(cfg.tmr_voter_clock_port_name, part.tmr_voter_clock_port_name);
        apply_if_present(cfg.tmr_voter_reset_port_name, part.tmr_voter_reset_port_name);
        apply_if_present(cfg.tmr_voter_clock_net, part.tmr_voter_clock_net);
        apply_if_present(cfg.tmr_voter_reset_net, part.tmr_voter_reset_net);
        apply_if_present(cfg.tmr_voter_safe_mode, part.tmr_voter_safe_mode);
        apply_if_present(cfg.preserve_module_ports, part.preserve_module_ports);
        apply_if_present(cfg.insert_voter_before_ff, part.insert_voter_before_ff);
        apply_if_present(cfg.insert_voter_after_ff, part.insert_voter_after_ff);
        apply_if_present(cfg.tmr_mode_full_module_insert_voter_before_modules,
                         part.tmr_mode_full_module_insert_voter_before_modules);
        apply_if_present(cfg.tmr_mode_full_module_insert_voter_after_modules,
                         part.tmr_mode_full_module_insert_voter_after_modules);
        apply_if_present(cfg.tmr_mode_full_module_insert_voter_on_clock_nets,
                         part.tmr_mode_full_module_insert_voter_on_clock_nets);
        apply_if_present(cfg.tmr_mode_full_module_insert_voter_on_reset_nets,
                         part.tmr_mode_full_module_insert_voter_on_reset_nets);
        apply_if_present(cfg.clock_port_names, part.clock_port_names);
        apply_if_present(cfg.expand_clock, part.expand_clock);
        apply_if_present(cfg.reset_port_names, part.reset_port_names);
        apply_if_present(cfg.expand_reset, part.expand_reset);
        apply_if_present(cfg.ff_cells, part.ff_cells);
        apply_if_present(cfg.additional_ff_cells, part.additional_ff_cells);
        apply_if_present(cfg.excluded_ff_cells, part.excluded_ff_cells);
        apply_if_present(cfg.logic_path_1_suffix, part.logic_path_1_suffix);
        apply_if_present(cfg.logic_path_2_suffix, part.logic_path_2_suffix);
        apply_if_present(cfg.logic_path_3_suffix, part.logic_path_3_suffix);
        apply_if_present(cfg.error_port_name, part.error_port_name);
    }
    return cfg;
}

void ConfigManager::append_group_configs(
    std::vector<ConfigPart> &cfg_parts,
    const Yosys::dict<Yosys::RTLIL::IdString, std::vector<std::string>> &group_assignments,
    Yosys::RTLIL::IdString lookup_name,
    Yosys::RTLIL::IdString log_name) {

    if (group_assignments.count(lookup_name) == 0) return;

    for (const auto &group_name : group_assignments.at(lookup_name)) {
        if (group_cfg.count(group_name) == 0) {
            Yosys::log_warning("Group %s for module %s not found, skipping assignment",
                               group_name.c_str(), log_name.c_str());
            continue;
        }
        cfg_parts.push_back(group_cfg.at(group_name));
    }
}

void ConfigManager::append_config_if_present(
    std::vector<ConfigPart> &cfg_parts,
    const Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> &cfg_map,
    Yosys::RTLIL::IdString key) {

    if (cfg_map.count(key) != 0) {
        cfg_parts.push_back(cfg_map.at(key));
    }
}

void ConfigManager::load_custom_voters(Yosys::RTLIL::Design *design) {
    Yosys::pool<std::string> loaded_files;

    for (auto &[mod_name, c] : final_module_cfgs) {
        if (c.tmr_voter != TmrVoter::Custom) continue;

        if (c.tmr_voter_file.empty())
            Yosys::log_error(
                "Module '%s' uses Custom voter but tmr_voter_file is not set.\n",
                mod_name.c_str());

        if (c.tmr_voter_module.empty())
            Yosys::log_error(
                "Module '%s' uses Custom voter but tmr_voter_module is not set.\n",
                mod_name.c_str());

        if (loaded_files.count(c.tmr_voter_file) == 0) {
            Yosys::log("Loading custom voter cell from '%s'\n", c.tmr_voter_file.c_str());
            Yosys::run_pass("read_verilog " + c.tmr_voter_file, design);
            // Convert any remaining behavioral code (always blocks present in
            // partially-synthesised netlists) to RTLIL cells so that the
            // subsequent techmap / dfflibmap passes can process them.
            Yosys::run_pass("proc", design);
            Yosys::run_pass("opt -fast", design);
            loaded_files.insert(c.tmr_voter_file);
        }

        // Validate the loaded module has the required 1-bit port interface.
        Yosys::RTLIL::IdString voter_id = Yosys::RTLIL::IdString("\\" + c.tmr_voter_module);
        Yosys::RTLIL::Module *voter_mod = design->module(voter_id);

        if (voter_mod == nullptr)
            Yosys::log_error(
                "Module '%s': custom voter module '%s' was not found in the design after "
                "loading '%s'.\n",
                mod_name.c_str(), c.tmr_voter_module.c_str(), c.tmr_voter_file.c_str());

        voter_mod->fixup_ports();

        const std::string voter_module_name = c.tmr_voter_module;
        auto check_port = [&](const std::string &port, bool expect_input) {
            Yosys::RTLIL::Wire *w = voter_mod->wire("\\" + port);
            if (w == nullptr)
                Yosys::log_error(
                    "Custom voter '%s': required port '%s' is missing.\n",
                    voter_module_name.c_str(), port.c_str());
            if (w->width != 1)
                Yosys::log_error(
                    "Custom voter '%s': port '%s' must be 1-bit (found %d-bit).\n",
                    voter_module_name.c_str(), port.c_str(), w->width);
            if (expect_input && !w->port_input)
                Yosys::log_error(
                    "Custom voter '%s': port '%s' must be an input.\n",
                    voter_module_name.c_str(), port.c_str());
            if (!expect_input && !w->port_output)
                Yosys::log_error(
                    "Custom voter '%s': port '%s' must be an output.\n",
                    voter_module_name.c_str(), port.c_str());
        };

        check_port("a",   true);
        check_port("b",   true);
        check_port("c",   true);
        check_port("y",   false);
        check_port("err", false);

        // Validate explicitly configured clock/reset port names.
        if (!c.tmr_voter_clock_port_name.empty())
            check_port(c.tmr_voter_clock_port_name, true);
        if (!c.tmr_voter_reset_port_name.empty())
            check_port(c.tmr_voter_reset_port_name, true);

        // Keep the template module under its original name — insert_voter will
        // clone it with a unique \tmrx_voter_<prefix>_w1 name per insertion
        // site.  Exempt the template itself from TMR so it is never expanded.
        Config exempt = global_cfg;
        exempt.tmr_mode = TmrMode::None;
        exempt.preserve_module_ports = true;
        exempt.tmr_mode_full_module_insert_voter_before_modules = false;
        exempt.tmr_mode_full_module_insert_voter_after_modules = false;
        final_module_cfgs[voter_id] = exempt;
        Yosys::log("Custom voter template '%s' exempted from TMR expansion.\n",
                   c.tmr_voter_module.c_str());
    }
}

void ConfigManager::validate_cfg(Yosys::RTLIL::Design *design) {
    bool any_expand_clock = false;
    bool any_expand_reset = false;

    for (auto module : design->modules()) {
        // Re-read after potential overrides from earlier iterations.
        Config &c = final_module_cfgs.count(module->name)
                        ? final_module_cfgs.at(module->name)
                        : global_cfg;
        const char *mod = module->name.c_str();
        bool is_blackbox = module->get_blackbox_attribute() ||
                           module->has_memories() || module->has_processes();

        // Check 1: blackbox modules may only use None or FullModuleTMR.
        if (is_blackbox && c.tmr_mode == TmrMode::LogicTMR) {
            Yosys::log_warning(
                "Module '%s' has the blackbox attribute but tmr_mode is LogicTMR. "
                "LogicTMR is not supported for black-box modules. "
                "Overriding tmr_mode to FullModuleTMR.\n", mod);
            c.tmr_mode = TmrMode::FullModuleTMR;
        }

        // Check 2: tmr_mode None requires preserve_module_ports.
        if (c.tmr_mode == TmrMode::None && !c.preserve_module_ports) {
            Yosys::log_warning(
                "Module '%s' has tmr_mode None but preserve_module_ports is false. "
                "Ports must be preserved when TMR is disabled. "
                "Overriding preserve_module_ports to true.\n", mod);
            c.preserve_module_ports = true;
        }

        // Check 3: preserve_module_ports with expanded clock or reset can insert voters on those nets.
        if (c.preserve_module_ports && (c.expand_clock || c.expand_reset)) {
            if (c.expand_clock) {
                Yosys::log_warning(
                    "Module '%s' has preserve_module_ports enabled together with expand_clock. "
                    "This may cause voters to be inserted on the clock net.\n", mod);
            }
            if (c.expand_reset) {
                Yosys::log_warning(
                    "Module '%s' has preserve_module_ports enabled together with expand_reset. "
                    "This may cause voters to be inserted on the reset net.\n", mod);
            }
        }

        // Check 5: insert_voter_before_ff is not implemented — catch early.
        if (c.insert_voter_before_ff) {
            Yosys::log_error(
                "Module '%s': insert_voter_before_ff is not yet implemented.\n", mod);
        }

        // Check 6: duplicate path suffixes cause wire name collisions.
        if (c.logic_path_1_suffix == c.logic_path_2_suffix ||
            c.logic_path_1_suffix == c.logic_path_3_suffix ||
            c.logic_path_2_suffix == c.logic_path_3_suffix) {
            Yosys::log_error(
                "Module '%s': logic path suffixes must be unique "
                "(got '%s', '%s', '%s').\n",
                mod, c.logic_path_1_suffix.c_str(),
                c.logic_path_2_suffix.c_str(),
                c.logic_path_3_suffix.c_str());
        }

        // Check 7: FullModuleTMR-only options are no-ops in LogicTMR / None mode.
        if (c.tmr_mode != TmrMode::FullModuleTMR) {
            if (c.tmr_mode_full_module_insert_voter_before_modules)
                Yosys::log_warning(
                    "Module '%s': tmr_mode_full_module_insert_voter_before_modules has no effect "
                    "outside of FullModuleTMR mode.\n", mod);
            if (c.tmr_mode_full_module_insert_voter_after_modules)
                Yosys::log_warning(
                    "Module '%s': tmr_mode_full_module_insert_voter_after_modules has no effect "
                    "outside of FullModuleTMR mode.\n", mod);
            if (c.tmr_mode_full_module_insert_voter_on_clock_nets)
                Yosys::log_warning(
                    "Module '%s': tmr_mode_full_module_insert_voter_on_clock_nets has no effect "
                    "outside of FullModuleTMR mode.\n", mod);
            if (c.tmr_mode_full_module_insert_voter_on_reset_nets)
                Yosys::log_warning(
                    "Module '%s': tmr_mode_full_module_insert_voter_on_reset_nets has no effect "
                    "outside of FullModuleTMR mode.\n", mod);
        }

        // Check 8: LogicTMR-only options are no-ops in FullModuleTMR mode.
        if (c.tmr_mode == TmrMode::FullModuleTMR) {
            if (c.insert_voter_after_ff)
                Yosys::log_warning(
                    "Module '%s': insert_voter_after_ff has no effect in FullModuleTMR mode.\n",
                    mod);
            if (!c.ff_cells.empty() || !c.additional_ff_cells.empty() ||
                !c.excluded_ff_cells.empty())
                Yosys::log_warning(
                    "Module '%s': ff_cells / additional_ff_cells / excluded_ff_cells have no "
                    "effect in FullModuleTMR mode.\n", mod);
        }

        // Check 9: voter_on_clock/reset_nets is a no-op when the net is not expanded.
        if (c.tmr_mode == TmrMode::FullModuleTMR) {
            if (c.tmr_mode_full_module_insert_voter_on_clock_nets && !c.expand_clock)
                Yosys::log_warning(
                    "Module '%s': tmr_mode_full_module_insert_voter_on_clock_nets is true but "
                    "expand_clock is false — the flag has no effect.\n", mod);
            if (c.tmr_mode_full_module_insert_voter_on_reset_nets && !c.expand_reset)
                Yosys::log_warning(
                    "Module '%s': tmr_mode_full_module_insert_voter_on_reset_nets is true but "
                    "expand_reset is false — the flag has no effect.\n", mod);

            // Check 10: both flags set — voters WILL be placed on the clock/reset net.
            if (c.tmr_mode_full_module_insert_voter_on_clock_nets && c.expand_clock)
                Yosys::log_warning(
                    "Module '%s': tmr_mode_full_module_insert_voter_on_clock_nets and "
                    "expand_clock are both enabled. Voters will be inserted on the clock net.\n",
                    mod);
            if (c.tmr_mode_full_module_insert_voter_on_reset_nets && c.expand_reset)
                Yosys::log_warning(
                    "Module '%s': tmr_mode_full_module_insert_voter_on_reset_nets and "
                    "expand_reset are both enabled. Voters will be inserted on the reset net.\n",
                    mod);
        }

        if (c.expand_clock) any_expand_clock = true;
        if (c.expand_reset) any_expand_reset = true;
    }

    // Check 4: any expand_clock/reset in the design can place voters on those nets.
    if (any_expand_clock) {
        Yosys::log_warning(
            "One or more modules have expand_clock enabled. "
            "This may cause voters to be placed on clock nets.\n");
    }
    if (any_expand_reset) {
        Yosys::log_warning(
            "One or more modules have expand_reset enabled. "
            "This may cause voters to be placed on reset nets.\n");
    }
}

ConfigManager::ConfigManager(Yosys::RTLIL::Design *design, const std::string &cfg_file) {
    Yosys::log_header(design, "Loading TMRX configuration\n");
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

    ConfigPart global_config_part;
    if (loaded_cfg && t.contains("global")) {
        global_config_part = parse_config(t.at("global"));
        global_cfg = assemble_config({global_config_part}, global_cfg);
    }

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



    Yosys::dict<Yosys::RTLIL::IdString, std::vector<std::string>> group_assignments;

    if (loaded_cfg && t.contains("module_groups")) {
        const auto &module_groups_table = toml::get<toml::value>(t.at("module_groups"));

        for (auto &[mod_name, group_value] : module_groups_table.as_table()) {
            std::string group_name = toml::get<std::string>(group_value);
            group_assignments["\\" + mod_name].push_back(group_name);
        }
    }

    for (auto module : design->modules()) {

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

    for (auto module : design->modules()) {
        if (!(module->has_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE))) {
            continue;
        }

        Yosys::RTLIL::IdString specific_mod_name = module->name;
        Yosys::RTLIL::IdString mod_name = specific_mod_name.str().substr(0, specific_mod_name.str().find('$'));

        std::vector<ConfigPart> cfg_parts = {global_config_part};

        // Add group configs for both generic and specific module names
        append_group_configs(cfg_parts, group_assignments, mod_name, specific_mod_name);
        append_group_configs(cfg_parts, group_assignments, specific_mod_name, specific_mod_name);

        // Add module-specific configs in precedence order
        append_config_if_present(cfg_parts, module_cfgs, mod_name);
        append_config_if_present(cfg_parts, module_attr_cfgs, specific_mod_name);
        append_config_if_present(cfg_parts, specific_module_cfgs, specific_mod_name);

        // TODO: validate cfg and enforce some values
        final_module_cfgs[specific_mod_name] = assemble_config(cfg_parts, global_cfg);
    }

    // Also assemble configs for non-submodule modules (e.g., the top-level chip).
    // These have no '$' in their name, so mod_name == specific_mod_name.
    for (auto module : design->modules()) {
        if (module->has_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE)) {
            continue;
        }

        Yosys::RTLIL::IdString mod_name = module->name;

        std::vector<ConfigPart> cfg_parts = {global_config_part};
        append_group_configs(cfg_parts, group_assignments, mod_name, mod_name);
        append_config_if_present(cfg_parts, module_cfgs, mod_name);
        append_config_if_present(cfg_parts, module_attr_cfgs, mod_name);

        final_module_cfgs[mod_name] = assemble_config(cfg_parts, global_cfg);
    }

    load_custom_voters(design);
    validate_cfg(design);
}

const Config *ConfigManager::cfg(Yosys::RTLIL::Module *mod) const {
    if (final_module_cfgs.count(mod->name) == 0) {
        return &global_cfg;
    }

    return &final_module_cfgs.at(mod->name);
}

std::string ConfigManager::cfg_as_string(Yosys::RTLIL::Module *mod) const {
    const Config *c = cfg(mod);
    std::string ret;

    ret += "TMR-Mode: " + tmr_mode_to_string(c->tmr_mode) + "\n";
    ret += "TMR-Voter: " + tmr_voter_to_string(c->tmr_voter) + "\n";
    if (c->tmr_voter == TmrVoter::Custom) {
        ret += "TMR-Voter File: " + c->tmr_voter_file + "\n";
        ret += "TMR-Voter Module: " + c->tmr_voter_module + "\n";
        if (!c->tmr_voter_clock_port_name.empty())
            ret += "TMR-Voter Clock Port: " + c->tmr_voter_clock_port_name + "\n";
        if (!c->tmr_voter_reset_port_name.empty())
            ret += "TMR-Voter Reset Port: " + c->tmr_voter_reset_port_name + "\n";
        if (!c->tmr_voter_clock_net.empty())
            ret += "TMR-Voter Clock Net: " + c->tmr_voter_clock_net + "\n";
        if (!c->tmr_voter_reset_net.empty())
            ret += "TMR-Voter Reset Net: " + c->tmr_voter_reset_net + "\n";
    }
    ret += "TMR-Voter Safe Mode: " + bool_to_string(c->tmr_voter_safe_mode) + "\n";
    ret += "Preserve Mod Ports: " + bool_to_string(c->preserve_module_ports) + "\n";
    ret += "Insert Voter before ff: " + bool_to_string(c->insert_voter_before_ff) + "\n";
    ret += "Insert Voter after ff: " + bool_to_string(c->insert_voter_after_ff) + "\n";
    ret += "Tmr Mode full; insert voter before module: " +
           bool_to_string(c->tmr_mode_full_module_insert_voter_before_modules) + "\n";
    ret += "Tmr Mode full; insert voter after module: " +
           bool_to_string(c->tmr_mode_full_module_insert_voter_after_modules) + "\n";
    ret += "Tmr Mode full; insert voter on clock nets: " +
           bool_to_string(c->tmr_mode_full_module_insert_voter_on_clock_nets) + "\n";
    ret += "Tmr Mode full; insert voter on reset nets: " +
           bool_to_string(c->tmr_mode_full_module_insert_voter_on_reset_nets) + "\n";
    ret += "Clock port names: " + pool_to_string(c->clock_port_names) + "\n";
    ret += "Expand Clock net: " + bool_to_string(c->expand_clock) + "\n";
    ret += "Reset port names: " + pool_to_string(c->reset_port_names) + "\n";
    ret += "Expand reset net: " + bool_to_string(c->expand_reset) + "\n";
    ret += "FF Cells: " + pool_to_string(c->ff_cells) + "\n";
    ret += "Additional FF Cells: " + pool_to_string(c->additional_ff_cells) + "\n";
    ret += "Excluded FF Cells: " + pool_to_string(c->excluded_ff_cells) + "\n";
    ret += "Logic path 1 suffix: " + c->logic_path_1_suffix + "\n";
    ret += "Logic path 2 suffix: " + c->logic_path_2_suffix + "\n";
    ret += "Logic path 3 suffix: " + c->logic_path_3_suffix + "\n";
    if (!c->error_port_name.empty())
        ret += "Error port name: " + c->error_port_name + "\n";

    return ret;
}
