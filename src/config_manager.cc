#include "config_manager.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

YOSYS_NAMESPACE_BEGIN
namespace TMRX {

namespace {

const std::set<std::string> &topLevelScopeNames() {
    static const std::set<std::string> keys = {
        cfg_global_scope_name,
        cfg_group_scope_name,
        cfg_module_scope_name,
        cfg_specific_module_scope_name,
    };
    return keys;
}

const std::set<std::string> &scopedConfigRootKeys() {
    static const std::set<std::string> keys = {
        cfg_tmr_mode_key_name,
        cfg_tmr_voter_key_name,
        cfg_tmr_voter_file_key_name,
        cfg_tmr_voter_module_key_name,
        cfg_tmr_voter_clock_port_name_key_name,
        cfg_tmr_voter_reset_port_name_key_name,
        cfg_tmr_voter_clock_net_key_name,
        cfg_tmr_voter_reset_net_key_name,
        cfg_tmr_voter_safe_mode_key_name,
        cfg_preserve_module_ports_key_name,
        cfg_prevent_renaming_key_name,
        cfg_clock_port_names_key_name,
        cfg_expand_clock_key_name,
        cfg_reset_port_names_key_name,
        cfg_expand_reset_key_name,
        cfg_error_port_name_key_name,
        cfg_auto_error_port_key_name,
        cfg_logic_scope_name,
        cfg_full_module_scope_name,
    };
    return keys;
}

const std::set<std::string> &scopedConfigRootKeysWithGroups() {
    static const std::set<std::string> keys = {
        cfg_tmr_mode_key_name,
        cfg_tmr_voter_key_name,
        cfg_tmr_voter_file_key_name,
        cfg_tmr_voter_module_key_name,
        cfg_tmr_voter_clock_port_name_key_name,
        cfg_tmr_voter_reset_port_name_key_name,
        cfg_tmr_voter_clock_net_key_name,
        cfg_tmr_voter_reset_net_key_name,
        cfg_tmr_voter_safe_mode_key_name,
        cfg_preserve_module_ports_key_name,
        cfg_prevent_renaming_key_name,
        cfg_clock_port_names_key_name,
        cfg_expand_clock_key_name,
        cfg_reset_port_names_key_name,
        cfg_expand_reset_key_name,
        cfg_error_port_name_key_name,
        cfg_auto_error_port_key_name,
        cfg_logic_scope_name,
        cfg_full_module_scope_name,
        cfg_groups_key_name,
    };
    return keys;
}

const std::set<std::string> &logicConfigKeys() {
    static const std::set<std::string> keys = {
        cfg_insert_voter_before_ff_key_name,
        cfg_insert_voter_after_ff_key_name,
        cfg_ff_cells_key_name,
        cfg_additional_ff_cells_key_name,
        cfg_excluded_ff_cells_key_name,
        cfg_logic_path_1_suffix_key_name,
        cfg_logic_path_2_suffix_key_name,
        cfg_logic_path_3_suffix_key_name,
    };
    return keys;
}

const std::set<std::string> &fullModuleConfigKeys() {
    static const std::set<std::string> keys = {
        cfg_insert_voter_before_modules_key_name,
        cfg_insert_voter_after_modules_key_name,
        cfg_insert_voter_on_clock_nets_key_name,
        cfg_insert_voter_on_reset_nets_key_name,
    };
    return keys;
}

void ensureTable(const toml::value &t, const std::string &context) {
    if (!t.is_table()) {
        Yosys::log_error("Config scope '%s' must be a TOML table.\n", context.c_str());
    }
}

void validateKnownKeys(const toml::value &t, const std::set<std::string> &allowedKeys,
                       const std::string &context) {
    ensureTable(t, context);

    for (const auto &[key, value] : t.as_table()) {
        if (allowedKeys.count(key) == 0) {
            Yosys::log_error("Unknown config key '%s' in scope '%s'.\n", key.c_str(),
                             context.c_str());
        }
        (void)value;
    }
}

void validateTopLevelScopes(const toml::value &t) {
    ensureTable(t, cfg_root_context_name);

    for (const auto &[scopeName, scopeValue] : t.as_table()) {
        if (topLevelScopeNames().count(scopeName) == 0) {
            Yosys::log_error("Unknown top-level config scope '%s'. Use [global], [group.<name>], "
                             "[module.<name>] or [specific_module.\"<name>\"].\n",
                             scopeName.c_str());
        }
        ensureTable(scopeValue, scopeName);
    }
}

void validateScopedConfigTable(const toml::value &t, const std::string &context, bool allowGroups) {
    validateKnownKeys(t, allowGroups ? scopedConfigRootKeysWithGroups() : scopedConfigRootKeys(),
                      context);

    if (t.contains(cfg_logic_scope_name)) {
        const auto &logicTable = t.at(cfg_logic_scope_name);
        validateKnownKeys(logicTable, logicConfigKeys(), context + "." + cfg_logic_scope_name);
    }

    if (t.contains(cfg_full_module_scope_name)) {
        const auto &fullModuleTable = t.at(cfg_full_module_scope_name);
        validateKnownKeys(fullModuleTable, fullModuleConfigKeys(),
                          context + "." + cfg_full_module_scope_name);
    }
}

ConfigPart parseCommonConfigPart(const toml::value &t) {
    ConfigPart cfg;

    cfg.tmrMode = parseTmrMode(toml::find_or<std::string>(t, cfg_tmr_mode_key_name, ""));
    cfg.tmrVoter = parseTmrVoter(toml::find_or<std::string>(t, cfg_tmr_voter_key_name, ""));

    cfg.tmrVoterSafeMode = tomlFindOptional<bool>(t, cfg_tmr_voter_safe_mode_key_name);
    cfg.preserveModulePorts = tomlFindOptional<bool>(t, cfg_preserve_module_ports_key_name);
    cfg.preventRenaming = tomlFindOptional<bool>(t, cfg_prevent_renaming_key_name);
    cfg.expandClock = tomlFindOptional<bool>(t, cfg_expand_clock_key_name);
    cfg.expandReset = tomlFindOptional<bool>(t, cfg_expand_reset_key_name);
    cfg.autoErrorPort = tomlFindOptional<bool>(t, cfg_auto_error_port_key_name);

    cfg.tmrVoterFile = tomlFindOptional<std::string>(t, cfg_tmr_voter_file_key_name);
    cfg.tmrVoterModule = tomlFindOptional<std::string>(t, cfg_tmr_voter_module_key_name);
    cfg.tmrVoterClockPortName =
        tomlFindOptional<std::string>(t, cfg_tmr_voter_clock_port_name_key_name);
    cfg.tmrVoterResetPortName =
        tomlFindOptional<std::string>(t, cfg_tmr_voter_reset_port_name_key_name);
    cfg.tmrVoterClockNet = tomlFindOptional<std::string>(t, cfg_tmr_voter_clock_net_key_name);
    cfg.tmrVoterResetNet = tomlFindOptional<std::string>(t, cfg_tmr_voter_reset_net_key_name);
    cfg.errorPortName = tomlFindOptional<std::string>(t, cfg_error_port_name_key_name);

    cfg.clockPortNames = tomlParseIdStringPool(t, cfg_clock_port_names_key_name);
    cfg.resetPortNames = tomlParseIdStringPool(t, cfg_reset_port_names_key_name);

    return cfg;
}

ConfigPart parseLogicConfigPart(const toml::value &t) {
    ConfigPart cfg;

    cfg.insertVoterBeforeFf = tomlFindOptional<bool>(t, cfg_insert_voter_before_ff_key_name);
    cfg.insertVoterAfterFf = tomlFindOptional<bool>(t, cfg_insert_voter_after_ff_key_name);

    cfg.logicPath1Suffix = tomlFindOptional<std::string>(t, cfg_logic_path_1_suffix_key_name);
    cfg.logicPath2Suffix = tomlFindOptional<std::string>(t, cfg_logic_path_2_suffix_key_name);
    cfg.logicPath3Suffix = tomlFindOptional<std::string>(t, cfg_logic_path_3_suffix_key_name);

    cfg.ffCells = tomlParseIdStringPool(t, cfg_ff_cells_key_name);
    cfg.additionalFfCells = tomlParseIdStringPool(t, cfg_additional_ff_cells_key_name);
    cfg.excludedFfCells = tomlParseIdStringPool(t, cfg_excluded_ff_cells_key_name);

    return cfg;
}

ConfigPart parseFullModuleConfigPart(const toml::value &t) {
    ConfigPart cfg;

    cfg.tmrModeFullModuleInsertVoterBeforeModules =
        tomlFindOptional<bool>(t, cfg_insert_voter_before_modules_key_name);
    cfg.tmrModeFullModuleInsertVoterAfterModules =
        tomlFindOptional<bool>(t, cfg_insert_voter_after_modules_key_name);
    cfg.tmrModeFullModuleInsertVoterOnClockNets =
        tomlFindOptional<bool>(t, cfg_insert_voter_on_clock_nets_key_name);
    cfg.tmrModeFullModuleInsertVoterOnResetNets =
        tomlFindOptional<bool>(t, cfg_insert_voter_on_reset_nets_key_name);

    return cfg;
}

template <typename T> void mergeOptionalField(std::optional<T> &dest, const std::optional<T> &src) {
    if (src) {
        dest = src;
    }
}

void mergeConfigPart(ConfigPart &dest, const ConfigPart &src) {
    mergeOptionalField(dest.tmrMode, src.tmrMode);
    mergeOptionalField(dest.tmrVoter, src.tmrVoter);
    mergeOptionalField(dest.tmrVoterFile, src.tmrVoterFile);
    mergeOptionalField(dest.tmrVoterModule, src.tmrVoterModule);
    mergeOptionalField(dest.tmrVoterClockPortName, src.tmrVoterClockPortName);
    mergeOptionalField(dest.tmrVoterResetPortName, src.tmrVoterResetPortName);
    mergeOptionalField(dest.tmrVoterClockNet, src.tmrVoterClockNet);
    mergeOptionalField(dest.tmrVoterResetNet, src.tmrVoterResetNet);
    mergeOptionalField(dest.tmrVoterSafeMode, src.tmrVoterSafeMode);
    mergeOptionalField(dest.preserveModulePorts, src.preserveModulePorts);
    mergeOptionalField(dest.preventRenaming, src.preventRenaming);
    mergeOptionalField(dest.insertVoterBeforeFf, src.insertVoterBeforeFf);
    mergeOptionalField(dest.insertVoterAfterFf, src.insertVoterAfterFf);
    mergeOptionalField(dest.tmrModeFullModuleInsertVoterBeforeModules,
                       src.tmrModeFullModuleInsertVoterBeforeModules);
    mergeOptionalField(dest.tmrModeFullModuleInsertVoterAfterModules,
                       src.tmrModeFullModuleInsertVoterAfterModules);
    mergeOptionalField(dest.tmrModeFullModuleInsertVoterOnClockNets,
                       src.tmrModeFullModuleInsertVoterOnClockNets);
    mergeOptionalField(dest.tmrModeFullModuleInsertVoterOnResetNets,
                       src.tmrModeFullModuleInsertVoterOnResetNets);
    mergeOptionalField(dest.clockPortNames, src.clockPortNames);
    mergeOptionalField(dest.expandClock, src.expandClock);
    mergeOptionalField(dest.resetPortNames, src.resetPortNames);
    mergeOptionalField(dest.expandReset, src.expandReset);
    mergeOptionalField(dest.ffCells, src.ffCells);
    mergeOptionalField(dest.additionalFfCells, src.additionalFfCells);
    mergeOptionalField(dest.excludedFfCells, src.excludedFfCells);
    mergeOptionalField(dest.logicPath1Suffix, src.logicPath1Suffix);
    mergeOptionalField(dest.logicPath2Suffix, src.logicPath2Suffix);
    mergeOptionalField(dest.logicPath3Suffix, src.logicPath3Suffix);
    mergeOptionalField(dest.errorPortName, src.errorPortName);
    mergeOptionalField(dest.autoErrorPort, src.autoErrorPort);
}

std::vector<std::string> parseGroups(const toml::value &t) {
    if (!t.contains(cfg_groups_key_name)) {
        return {};
    }
    return toml::find<std::vector<std::string>>(t, cfg_groups_key_name);
}

void appendGroupsToAssignments(
    Yosys::dict<Yosys::RTLIL::IdString, std::vector<std::string>> &groupAssignments,
    Yosys::RTLIL::IdString modName, const std::vector<std::string> &groups) {

    for (const auto &groupName : groups) {
        groupAssignments[modName].push_back(groupName);
    }
}

std::string scopeContext(const std::string &scopeName, const std::string &entryName) {
    return scopeName + "." + entryName;
}

std::string quotedScopeContext(const std::string &scopeName, const std::string &entryName) {
    return scopeName + ".\"" + entryName + "\"";
}

} // namespace

// ============================================================================
// String conversion helpers
// ============================================================================

std::optional<TmrMode> parseTmrMode(const std::string &str) {
    if (str.empty())
        return std::nullopt;
    if (str == cfg_tmr_mode_none_name)
        return TmrMode::None;
    if (str == cfg_tmr_mode_full_module_tmr_name)
        return TmrMode::FullModuleTMR;
    if (str == cfg_tmr_mode_logic_tmr_name)
        return TmrMode::LogicTMR;
    return std::nullopt;
}

std::string tmrModeToString(TmrMode mode) {
    switch (mode) {
    case TmrMode::None:
        return cfg_tmr_mode_none_name;
    case TmrMode::FullModuleTMR:
        return cfg_tmr_mode_full_module_tmr_name;
    case TmrMode::LogicTMR:
        return cfg_tmr_mode_logic_tmr_name;
    }
    return cfg_unknown_name;
}

std::optional<TmrVoter> parseTmrVoter(const std::string &str) {
    if (str.empty())
        return std::nullopt;
    if (str == cfg_tmr_voter_default_name)
        return TmrVoter::Default;
    if (str == cfg_tmr_voter_custom_name)
        return TmrVoter::Custom;
    return std::nullopt;
}

std::string tmrVoterToString(TmrVoter voter) {
    switch (voter) {
    case TmrVoter::Default:
        return cfg_tmr_voter_default_name;
    case TmrVoter::Custom:
        return cfg_tmr_voter_custom_name;
    }
    return cfg_unknown_name;
}

// ============================================================================
// TOML parsing helpers
// ============================================================================

template <typename T>
std::optional<T> tomlFindOptional(const toml::value &t, const std::string &key) {
    if (t.contains(key)) {
        return toml::find<T>(t, key);
    }
    return std::nullopt;
}

// Explicit template instantiations
template std::optional<bool> tomlFindOptional<bool>(const toml::value &t, const std::string &key);
template std::optional<std::string> tomlFindOptional<std::string>(const toml::value &t,
                                                                  const std::string &key);

std::optional<Yosys::pool<Yosys::RTLIL::IdString>> tomlParseIdStringPool(const toml::value &t,
                                                                         const std::string &key) {
    if (!t.contains(key))
        return std::nullopt;

    auto items = toml::find_or<std::vector<std::string>>(t, key, {});
    Yosys::pool<Yosys::RTLIL::IdString> pool;
    for (const auto &item : items) {
        pool.insert(makeRtlilId(item));
    }
    return pool;
}

// ============================================================================
// Config assembly helper
// ============================================================================

template <typename T> void applyIfPresent(T &dest, const std::optional<T> &src) {
    if (src) {
        dest = *src;
    }
}

// Explicit template instantiations for common types
template void applyIfPresent<TmrMode>(TmrMode &dest, const std::optional<TmrMode> &src);
template void applyIfPresent<TmrVoter>(TmrVoter &dest, const std::optional<TmrVoter> &src);
template void applyIfPresent<bool>(bool &dest, const std::optional<bool> &src);
template void applyIfPresent<std::string>(std::string &dest, const std::optional<std::string> &src);
template void applyIfPresent<Yosys::pool<Yosys::RTLIL::IdString>>(
    Yosys::pool<Yosys::RTLIL::IdString> &dest,
    const std::optional<Yosys::pool<Yosys::RTLIL::IdString>> &src);

// ============================================================================
// String formatting helpers
// ============================================================================

std::string poolToString(const Yosys::pool<Yosys::RTLIL::IdString> &pool) {
    std::string ret = "[";
    for (const auto &item : pool) {
        ret += item.str() + ", ";
    }
    ret += "]";
    return ret;
}

std::string boolToString(bool val) { return val ? "true" : "false"; }

// ============================================================================
// ConfigManager implementation
// ============================================================================

void ConfigManager::loadGlobalDefaultCfg() {
    globalCfg = {};

    globalCfg.tmrMode = TmrMode::LogicTMR;
    globalCfg.tmrVoter = TmrVoter::Default;
    globalCfg.tmrVoterFile = "";
    globalCfg.tmrVoterModule = "";
    globalCfg.tmrVoterClockPortName = "";
    globalCfg.tmrVoterResetPortName = "";
    globalCfg.tmrVoterClockNet = "";
    globalCfg.tmrVoterResetNet = "";
    globalCfg.tmrVoterSafeMode = true;

    globalCfg.preserveModulePorts = false;
    globalCfg.preventRenaming = false;

    globalCfg.insertVoterBeforeFf = false;
    globalCfg.insertVoterAfterFf = true;

    globalCfg.tmrModeFullModuleInsertVoterBeforeModules = false;
    globalCfg.tmrModeFullModuleInsertVoterAfterModules = true;
    globalCfg.tmrModeFullModuleInsertVoterOnClockNets = false;
    globalCfg.tmrModeFullModuleInsertVoterOnResetNets = false;

    globalCfg.clockPortNames = {Yosys::RTLIL::IdString(cfg_default_clock_port_name)};
    globalCfg.resetPortNames = {Yosys::RTLIL::IdString(cfg_default_reset_port_name)};

    globalCfg.expandClock = false;
    globalCfg.expandReset = false;

    globalCfg.ffCells = {};
    globalCfg.additionalFfCells = {};
    globalCfg.excludedFfCells = {};

    globalCfg.logicPath1Suffix = cfg_default_logic_path_1_suffix;
    globalCfg.logicPath2Suffix = cfg_default_logic_path_2_suffix;
    globalCfg.logicPath3Suffix = cfg_default_logic_path_3_suffix;

    globalCfg.errorPortName = "";
    globalCfg.autoErrorPort = false;
}

void ConfigManager::loadDefaultGroupsCfg() {
    // TODO: parmatrize
    groupCfg[cfg_default_black_box_module_group_name] = ConfigPart();
    groupCfg[cfg_default_black_box_module_group_name].tmrMode = TmrMode::FullModuleTMR;

    groupCfg[cfg_default_cdc_module_group_name] = ConfigPart();
    groupCfg[cfg_default_cdc_module_group_name].tmrMode = TmrMode::FullModuleTMR;

    // TODO: add top module
}

std::string ConfigManager::getStringAttrValueOr(const Yosys::RTLIL::Module *mod,
                                                const std::string &attr, const std::string &def) {
    std::string ret = def;
    if (mod->has_attribute(attr)) {
        ret = mod->get_string_attribute(attr);
    }
    return ret;
}
bool ConfigManager::getBoolAttrValueOr(const Yosys::RTLIL::Module *mod, const std::string &attr,
                                       bool def) {
    bool ret = def;
    if (mod->has_attribute(attr)) {
        ret = (mod->get_string_attribute(attr)) == cfg_true_value;
    }
    return ret;
}
int ConfigManager::getIntAttrValueOr(const Yosys::RTLIL::Module *mod, const std::string &attr,
                                     int def) {
    int ret = def;
    if (mod->has_attribute(attr)) {
        ret = std::stoi(mod->get_string_attribute(attr));
    }
    return ret;
}

std::optional<std::string> ConfigManager::getStringAttrValue(const Yosys::RTLIL::Module *mod,
                                                             const std::string &attr) {
    if (mod->has_attribute(attr)) {
        return mod->get_string_attribute(attr);
    }
    return std::nullopt;
}
std::optional<bool> ConfigManager::getBoolAttrValue(const Yosys::RTLIL::Module *mod,
                                                    const std::string &attr) {
    if (mod->has_attribute(attr)) {
        return (mod->get_string_attribute(attr)) == cfg_true_value;
    }
    return std::nullopt;
}
std::optional<int> ConfigManager::getIntAttrValue(const Yosys::RTLIL::Module *mod,
                                                  const std::string &attr) {
    if (mod->has_attribute(attr)) {
        return std::stoi(mod->get_string_attribute(attr));
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>>
ConfigManager::getStringListAttrValue(const Yosys::RTLIL::Module *mod, const std::string &attr) {
    if (mod->has_attribute(attr)) {
        std::vector<std::string> res;
        std::string attrList = mod->get_string_attribute(attr);
        std::stringstream ss(attrList);
        std::string attri;

        while (std::getline(ss, attri, cfg_attr_list_separator)) {
            res.push_back(attri);
        }
        return res;
    }
    return std::nullopt;
}

ConfigPart ConfigManager::parseConfig(const toml::value &t) {
    ConfigPart cfg = parseCommonConfigPart(t);

    if (t.contains(cfg_logic_scope_name)) {
        mergeConfigPart(cfg, parseLogicConfigPart(t.at(cfg_logic_scope_name)));
    }

    if (t.contains(cfg_full_module_scope_name)) {
        mergeConfigPart(cfg, parseFullModuleConfigPart(t.at(cfg_full_module_scope_name)));
    }

    return cfg;
}

std::optional<Yosys::pool<Yosys::RTLIL::IdString>>
ConfigManager::parseAttrIdStringPool(const Yosys::RTLIL::Module *mod, const std::string &attr) {
    auto strs = getStringListAttrValue(mod, attr);
    if (!strs)
        return std::nullopt;

    Yosys::pool<Yosys::RTLIL::IdString> pool;
    for (const auto &s : *strs) {
        pool.insert(makeRtlilId(s));
    }
    return pool;
}

ConfigPart ConfigManager::parseModuleAnnotations(const Yosys::RTLIL::Module *mod) {
    ConfigPart cfg;

    // Parse enum fields
    cfg.tmrMode = parseTmrMode(getStringAttrValueOr(mod, cfg_tmr_mode_attr_name, ""));
    cfg.tmrVoter = parseTmrVoter(getStringAttrValueOr(mod, cfg_tmr_voter_attr_name, ""));

    // Parse boolean fields
    cfg.tmrVoterSafeMode = getBoolAttrValue(mod, cfg_tmr_voter_safe_mode_attr_name);
    cfg.preserveModulePorts = getBoolAttrValue(mod, cfg_tmr_preserve_module_ports_attr_name);
    cfg.preventRenaming = getBoolAttrValue(mod, cfg_prevent_renaming_attr_name);
    cfg.insertVoterBeforeFf = getBoolAttrValue(mod, cfg_insert_voter_before_ff_attr_name);
    cfg.insertVoterAfterFf = getBoolAttrValue(mod, cfg_insert_voter_after_ff_attr_name);
    cfg.tmrModeFullModuleInsertVoterBeforeModules =
        getBoolAttrValue(mod, cfg_tmr_mode_full_module_insert_voter_before_modules_attr_name);
    cfg.tmrModeFullModuleInsertVoterAfterModules =
        getBoolAttrValue(mod, cfg_tmr_mode_full_module_insert_voter_after_modules_attr_name);
    cfg.tmrModeFullModuleInsertVoterOnClockNets =
        getBoolAttrValue(mod, cfg_tmr_mode_full_module_insert_voter_on_clock_nets_attr_name);
    cfg.tmrModeFullModuleInsertVoterOnResetNets =
        getBoolAttrValue(mod, cfg_tmr_mode_full_module_insert_voter_on_reset_nets_attr_name);
    cfg.expandClock = getBoolAttrValue(mod, cfg_expand_clock_attr_name);
    cfg.expandReset = getBoolAttrValue(mod, cfg_expand_rst_attr_name);

    // Parse string fields
    cfg.tmrVoterFile = getStringAttrValue(mod, cfg_tmr_voter_file_attr_name);
    cfg.tmrVoterModule = getStringAttrValue(mod, cfg_tmr_voter_module_attr_name);
    cfg.tmrVoterClockPortName = getStringAttrValue(mod, cfg_tmr_voter_clock_port_name_attr_name);
    cfg.tmrVoterResetPortName = getStringAttrValue(mod, cfg_tmr_voter_reset_port_name_attr_name);
    cfg.tmrVoterClockNet = getStringAttrValue(mod, cfg_tmr_voter_clock_net_attr_name);
    cfg.tmrVoterResetNet = getStringAttrValue(mod, cfg_tmr_voter_reset_net_attr_name);
    cfg.logicPath1Suffix = getStringAttrValue(mod, cfg_logic_path_1_suffix_attr_name);
    cfg.logicPath2Suffix = getStringAttrValue(mod, cfg_logic_path_2_suffix_attr_name);
    cfg.logicPath3Suffix = getStringAttrValue(mod, cfg_logic_path_3_suffix_attr_name);
    cfg.errorPortName = getStringAttrValue(mod, cfg_error_port_name_attr_name);
    cfg.autoErrorPort = getBoolAttrValue(mod, cfg_auto_error_port_attr_name);

    // Parse IdString pool fields
    cfg.clockPortNames = parseAttrIdStringPool(mod, cfg_clock_port_name_attr_name);
    cfg.resetPortNames = parseAttrIdStringPool(mod, cfg_rst_port_name_attr_name);

    return cfg;
}

Config ConfigManager::assembleConfig(std::vector<ConfigPart> parts, Config def) {
    Config cfg(def);
    for (const auto &part : parts) {
        applyIfPresent(cfg.tmrMode, part.tmrMode);
        applyIfPresent(cfg.tmrVoter, part.tmrVoter);
        applyIfPresent(cfg.tmrVoterFile, part.tmrVoterFile);
        applyIfPresent(cfg.tmrVoterModule, part.tmrVoterModule);
        applyIfPresent(cfg.tmrVoterClockPortName, part.tmrVoterClockPortName);
        applyIfPresent(cfg.tmrVoterResetPortName, part.tmrVoterResetPortName);
        applyIfPresent(cfg.tmrVoterClockNet, part.tmrVoterClockNet);
        applyIfPresent(cfg.tmrVoterResetNet, part.tmrVoterResetNet);
        applyIfPresent(cfg.tmrVoterSafeMode, part.tmrVoterSafeMode);
        applyIfPresent(cfg.preserveModulePorts, part.preserveModulePorts);
        applyIfPresent(cfg.preventRenaming, part.preventRenaming);
        applyIfPresent(cfg.insertVoterBeforeFf, part.insertVoterBeforeFf);
        applyIfPresent(cfg.insertVoterAfterFf, part.insertVoterAfterFf);
        applyIfPresent(cfg.tmrModeFullModuleInsertVoterBeforeModules,
                       part.tmrModeFullModuleInsertVoterBeforeModules);
        applyIfPresent(cfg.tmrModeFullModuleInsertVoterAfterModules,
                       part.tmrModeFullModuleInsertVoterAfterModules);
        applyIfPresent(cfg.tmrModeFullModuleInsertVoterOnClockNets,
                       part.tmrModeFullModuleInsertVoterOnClockNets);
        applyIfPresent(cfg.tmrModeFullModuleInsertVoterOnResetNets,
                       part.tmrModeFullModuleInsertVoterOnResetNets);
        applyIfPresent(cfg.clockPortNames, part.clockPortNames);
        applyIfPresent(cfg.expandClock, part.expandClock);
        applyIfPresent(cfg.resetPortNames, part.resetPortNames);
        applyIfPresent(cfg.expandReset, part.expandReset);
        applyIfPresent(cfg.ffCells, part.ffCells);
        applyIfPresent(cfg.additionalFfCells, part.additionalFfCells);
        applyIfPresent(cfg.excludedFfCells, part.excludedFfCells);
        applyIfPresent(cfg.logicPath1Suffix, part.logicPath1Suffix);
        applyIfPresent(cfg.logicPath2Suffix, part.logicPath2Suffix);
        applyIfPresent(cfg.logicPath3Suffix, part.logicPath3Suffix);
        applyIfPresent(cfg.errorPortName, part.errorPortName);
        applyIfPresent(cfg.autoErrorPort, part.autoErrorPort);
    }
    return cfg;
}

void ConfigManager::appendGroupConfigs(
    std::vector<ConfigPart> &cfgParts,
    const Yosys::dict<Yosys::RTLIL::IdString, std::vector<std::string>> &groupAssignments,
    Yosys::RTLIL::IdString lookupName, Yosys::RTLIL::IdString logName) {

    if (groupAssignments.count(lookupName) == 0)
        return;

    for (const auto &groupName : groupAssignments.at(lookupName)) {
        if (groupCfg.count(groupName) == 0) {
            Yosys::log_warning("Group %s for module %s not found, skipping assignment",
                               groupName.c_str(), logName.c_str());
            continue;
        }
        cfgParts.push_back(groupCfg.at(groupName));
    }
}

void ConfigManager::appendConfigIfPresent(
    std::vector<ConfigPart> &cfgParts,
    const Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> &cfgMap, Yosys::RTLIL::IdString key) {

    if (cfgMap.count(key) != 0) {
        cfgParts.push_back(cfgMap.at(key));
    }
}

void ConfigManager::loadCustomVoters(Yosys::RTLIL::Design *design) {
    Yosys::pool<std::string> loaded_files;

    for (auto &[modName, c] : finalModuleCfgs) {
        if (c.tmrVoter != TmrVoter::Custom)
            continue;

        if (c.tmrVoterFile.empty())
            Yosys::log_error("Module '%s' uses Custom voter but tmr_voter_file is not set.\n",
                             modName.c_str());

        if (c.tmrVoterModule.empty())
            Yosys::log_error("Module '%s' uses Custom voter but tmr_voter_module is not set.\n",
                             modName.c_str());

        if (loaded_files.count(c.tmrVoterFile) == 0) {
            Yosys::log("Loading custom voter cell from '%s'\n", c.tmrVoterFile.c_str());
            Yosys::run_pass("read_verilog " + c.tmrVoterFile, design);
            // Convert any remaining behavioral code (always blocks present in
            // partially-synthesised netlists) to RTLIL cells so that the
            // subsequent techmap / dfflibmap passes can process them.
            Yosys::run_pass("proc", design);
            Yosys::run_pass("opt -fast", design);
            loaded_files.insert(c.tmrVoterFile);
        }

        // Validate the loaded module has the required 1-bit port interface.
        Yosys::RTLIL::IdString voterId = makeRtlilId(c.tmrVoterModule);
        Yosys::RTLIL::Module *voterMod = design->module(voterId);

        if (voterMod == nullptr)
            Yosys::log_error(
                "Module '%s': custom voter module '%s' was not found in the design after "
                "loading '%s'.\n",
                modName.c_str(), c.tmrVoterModule.c_str(), c.tmrVoterFile.c_str());

        voterMod->fixup_ports();

        const std::string voterModuleName = c.tmrVoterModule;
        auto checkPort = [&](const std::string &port, bool expectInput) {
            Yosys::RTLIL::Wire *w = voterMod->wire(makeRtlilId(port));
            if (w == nullptr)
                Yosys::log_error("Custom voter '%s': required port '%s' is missing.\n",
                                 voterModuleName.c_str(), port.c_str());
            if (w->width != 1)
                Yosys::log_error("Custom voter '%s': port '%s' must be 1-bit (found %d-bit).\n",
                                 voterModuleName.c_str(), port.c_str(), w->width);
            if (expectInput && !w->port_input)
                Yosys::log_error("Custom voter '%s': port '%s' must be an input.\n",
                                 voterModuleName.c_str(), port.c_str());
            if (!expectInput && !w->port_output)
                Yosys::log_error("Custom voter '%s': port '%s' must be an output.\n",
                                 voterModuleName.c_str(), port.c_str());
        };

        checkPort(tmrx_voter_port_a_name, true);
        checkPort(tmrx_voter_port_b_name, true);
        checkPort(tmrx_voter_port_c_name, true);
        checkPort(tmrx_voter_port_y_name, false);
        checkPort(tmrx_voter_port_err_name, false);

        // Validate explicitly configured clock/reset port names.
        if (!c.tmrVoterClockPortName.empty())
            checkPort(c.tmrVoterClockPortName, true);
        if (!c.tmrVoterResetPortName.empty())
            checkPort(c.tmrVoterResetPortName, true);

        // Keep the template module under its original name — insertVoter will
        // clone it with a unique \tmrx_voter_<prefix>_w1 name per insertion
        // site.  Exempt the template itself from TMR so it is never expanded.
        Config exempt = globalCfg;
        exempt.tmrMode = TmrMode::None;
        exempt.preserveModulePorts = true;
        exempt.tmrModeFullModuleInsertVoterBeforeModules = false;
        exempt.tmrModeFullModuleInsertVoterAfterModules = false;
        finalModuleCfgs[voterId] = exempt;
        Yosys::log("Custom voter template '%s' exempted from TMR expansion.\n",
                   c.tmrVoterModule.c_str());
    }
}

void ConfigManager::validateCfg(Yosys::RTLIL::Design *design) {
    bool anyExpandClock = false;
    bool anyExpandReset = false;

    for (auto module : design->modules()) {
        // Re-read after potential overrides from earlier iterations.
        Config &c =
            finalModuleCfgs.count(module->name) ? finalModuleCfgs.at(module->name) : globalCfg;
        const char *mod = module->name.c_str();
        bool isBlackbox =
            module->get_blackbox_attribute() || module->has_memories() || module->has_processes();

        // Check 1: blackbox modules may only use None or FullModuleTMR.
        if (isBlackbox && c.tmrMode == TmrMode::LogicTMR) {
            Yosys::log_warning("Module '%s' has the blackbox attribute but tmr_mode is LogicTMR. "
                               "LogicTMR is not supported for black-box modules. "
                               "Overriding tmr_mode to FullModuleTMR.\n",
                               mod);
            c.tmrMode = TmrMode::FullModuleTMR;
        }

        // Check 2: tmr_mode None requires preserve_module_ports.
        if (c.tmrMode == TmrMode::None && !c.preserveModulePorts) {
            Yosys::log_warning("Module '%s' has tmr_mode None but preserve_module_ports is false. "
                               "Ports must be preserved when TMR is disabled. "
                               "Overriding preserve_module_ports to true.\n",
                               mod);
            c.preserveModulePorts = true;
        }

        // Check 3: preserve_module_ports with expanded clock or reset can insert voters on those
        // nets.
        if (c.preserveModulePorts && (c.expandClock || c.expandReset)) {
            if (c.expandClock) {
                Yosys::log_warning(
                    "Module '%s' has preserve_module_ports enabled together with expand_clock. "
                    "This may cause voters to be inserted on the clock net.\n",
                    mod);
            }
            if (c.expandReset) {
                Yosys::log_warning(
                    "Module '%s' has preserve_module_ports enabled together with expand_reset. "
                    "This may cause voters to be inserted on the reset net.\n",
                    mod);
            }
        }

        // Check 4: preserve_module_ports with auto_error_port can still add a new shared
        // error output to the preserved interface.
        if (c.preserveModulePorts && c.autoErrorPort) {
            Yosys::log_warning(
                "Module '%s' has preserve_module_ports enabled together with auto_error_port. "
                "If voter errors exist, TMRX will still auto-create a shared error output "
                "port.\n",
                mod);
        }

        // Check 5: insert_voter_before_ff is not implemented — catch early.
        if (c.insertVoterBeforeFf) {
            Yosys::log_error("Module '%s': insert_voter_before_ff is not yet implemented.\n", mod);
        }

        // Check 6: duplicate path suffixes cause wire name collisions.
        if (c.logicPath1Suffix == c.logicPath2Suffix || c.logicPath1Suffix == c.logicPath3Suffix ||
            c.logicPath2Suffix == c.logicPath3Suffix) {
            Yosys::log_error("Module '%s': logic path suffixes must be unique "
                             "(got '%s', '%s', '%s').\n",
                             mod, c.logicPath1Suffix.c_str(), c.logicPath2Suffix.c_str(),
                             c.logicPath3Suffix.c_str());
        }

        // Check 7: FullModuleTMR-only options are no-ops in LogicTMR / None mode.
        if (c.tmrMode != TmrMode::FullModuleTMR) {
            if (c.tmrModeFullModuleInsertVoterBeforeModules)
                Yosys::log_warning(
                    "Module '%s': full_module.insert_voter_before_modules has no effect "
                    "outside of FullModuleTMR mode.\n",
                    mod);
            if (c.tmrModeFullModuleInsertVoterAfterModules)
                Yosys::log_warning(
                    "Module '%s': full_module.insert_voter_after_modules has no effect "
                    "outside of FullModuleTMR mode.\n",
                    mod);
            if (c.tmrModeFullModuleInsertVoterOnClockNets)
                Yosys::log_warning(
                    "Module '%s': full_module.insert_voter_on_clock_nets has no effect "
                    "outside of FullModuleTMR mode.\n",
                    mod);
            if (c.tmrModeFullModuleInsertVoterOnResetNets)
                Yosys::log_warning(
                    "Module '%s': full_module.insert_voter_on_reset_nets has no effect "
                    "outside of FullModuleTMR mode.\n",
                    mod);
        }

        // Check 8: LogicTMR-only options are no-ops in FullModuleTMR mode.
        if (c.tmrMode == TmrMode::FullModuleTMR) {
            if (c.insertVoterAfterFf)
                Yosys::log_warning(
                    "Module '%s': insert_voter_after_ff has no effect in FullModuleTMR mode.\n",
                    mod);
            if (!c.ffCells.empty() || !c.additionalFfCells.empty() || !c.excludedFfCells.empty())
                Yosys::log_warning(
                    "Module '%s': ff_cells / additional_ff_cells / excluded_ff_cells have no "
                    "effect in FullModuleTMR mode.\n",
                    mod);
        }

        // Check 9: voter_on_clock/reset_nets is a no-op when the net is not expanded.
        if (c.tmrMode == TmrMode::FullModuleTMR) {
            if (c.tmrModeFullModuleInsertVoterOnClockNets && !c.expandClock)
                Yosys::log_warning(
                    "Module '%s': full_module.insert_voter_on_clock_nets is true but "
                    "expand_clock is false — the flag has no effect.\n",
                    mod);
            if (c.tmrModeFullModuleInsertVoterOnResetNets && !c.expandReset)
                Yosys::log_warning(
                    "Module '%s': full_module.insert_voter_on_reset_nets is true but "
                    "expand_reset is false — the flag has no effect.\n",
                    mod);

            // Check 10: both flags set — voters WILL be placed on the clock/reset net.
            if (c.tmrModeFullModuleInsertVoterOnClockNets && c.expandClock)
                Yosys::log_warning(
                    "Module '%s': full_module.insert_voter_on_clock_nets and "
                    "expand_clock are both enabled. Voters will be inserted on the clock net.\n",
                    mod);
            if (c.tmrModeFullModuleInsertVoterOnResetNets && c.expandReset)
                Yosys::log_warning(
                    "Module '%s': full_module.insert_voter_on_reset_nets and "
                    "expand_reset are both enabled. Voters will be inserted on the reset net.\n",
                    mod);
        }

        if (c.expandClock)
            anyExpandClock = true;
        if (c.expandReset)
            anyExpandReset = true;
    }

    // Check 4: any expand_clock/reset in the design can place voters on those nets.
    if (anyExpandClock) {
        Yosys::log_warning("One or more modules have expand_clock enabled. "
                           "This may cause voters to be placed on clock nets.\n");
    }
    if (anyExpandReset) {
        Yosys::log_warning("One or more modules have expand_reset enabled. "
                           "This may cause voters to be placed on reset nets.\n");
    }
}

ConfigManager::ConfigManager(Yosys::RTLIL::Design *design, const std::string &cfgFile) {
    Yosys::log_header(design, "Loading TMRX configuration\n");
    loadGlobalDefaultCfg();
    loadDefaultGroupsCfg();

    bool loadedCfg = true;

    toml::value t;
    try {
        t = toml::parse(cfgFile);
    } catch (const std::exception &e) {
        Yosys::log_warning("Tmrx was unable to load cfg file, will proceed with default config and "
                           "annotation '%s': %s. Using defaults.",
                           cfgFile.c_str(), e.what());
        loadedCfg = false;
    }

    if (t.is_empty()) {
        Yosys::log_warning(
            "Config file empty, tmrx will proceed with default config and annotation");
    }

    ConfigPart globalConfigPart;
    Yosys::dict<Yosys::RTLIL::IdString, std::vector<std::string>> groupAssignments;

    if (loadedCfg) {
        validateTopLevelScopes(t);

        if (t.contains(cfg_global_scope_name)) {
            const auto &globalTable = t.at(cfg_global_scope_name);
            validateScopedConfigTable(globalTable, cfg_global_scope_name, false);
            globalConfigPart = parseConfig(globalTable);
            globalCfg = assembleConfig({globalConfigPart}, globalCfg);
        }

        if (t.contains(cfg_group_scope_name)) {
            const auto &groupRoot = t.at(cfg_group_scope_name);
            ensureTable(groupRoot, cfg_group_scope_name);

            for (const auto &[groupName, groupValue] : groupRoot.as_table()) {
                validateScopedConfigTable(groupValue, scopeContext(cfg_group_scope_name, groupName),
                                          false);
                groupCfg[groupName] = parseConfig(groupValue);
            }
        }

        if (t.contains(cfg_module_scope_name)) {
            const auto &moduleRoot = t.at(cfg_module_scope_name);
            ensureTable(moduleRoot, cfg_module_scope_name);

            for (const auto &[moduleName, moduleValue] : moduleRoot.as_table()) {
                validateScopedConfigTable(moduleValue,
                                          scopeContext(cfg_module_scope_name, moduleName), true);

                Yosys::RTLIL::IdString moduleId = makeRtlilId(moduleName);
                appendGroupsToAssignments(groupAssignments, moduleId, parseGroups(moduleValue));
                moduleCfgs[moduleId] = parseConfig(moduleValue);
            }
        }

        if (t.contains(cfg_specific_module_scope_name)) {
            const auto &specificModuleRoot = t.at(cfg_specific_module_scope_name);
            ensureTable(specificModuleRoot, cfg_specific_module_scope_name);

            for (const auto &[moduleName, moduleValue] : specificModuleRoot.as_table()) {
                if (moduleName.find(cfg_specific_module_separator) == std::string::npos) {
                    Yosys::log_error("Specific module configurations require a '$' in the name. "
                                     "Use [module.<name>] for generic module configuration.\n");
                }

                validateScopedConfigTable(
                    moduleValue, quotedScopeContext(cfg_specific_module_scope_name, moduleName),
                    true);

                Yosys::RTLIL::IdString moduleId = makeRtlilId(moduleName);
                appendGroupsToAssignments(groupAssignments, moduleId, parseGroups(moduleValue));
                specificModuleCfgs[moduleId] = parseConfig(moduleValue);
            }
        }
    }

    for (auto module : design->modules()) {

        Yosys::RTLIL::IdString modName = module->name;

        auto groupNames = getStringListAttrValue(module, cfg_group_assignment_attr_name);

        if (groupNames) {
            for (const auto &name : *groupNames) {
                groupAssignments[modName].push_back(name);
            }
        }

        if (module->has_memories() || module->has_processes()) {

            groupAssignments[modName].push_back(cfg_default_black_box_module_group_name);
            Yosys::log_warning(
                "Module %s contains memories or processes, module will be treated as black box",
                modName.c_str());
        }

        if (module->get_blackbox_attribute()) {
            groupAssignments[modName].push_back(cfg_default_black_box_module_group_name);
        }

        // Yosys::log_header(design, "creating id string");
        moduleAttrCfgs[modName] = parseModuleAnnotations(module);
    }

    for (auto module : design->modules()) {
        if (!(module->has_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE))) {
            continue;
        }

        Yosys::RTLIL::IdString specificModName = module->name;
        Yosys::RTLIL::IdString modName = specificModName.str().substr(
            0, specificModName.str().find(cfg_specific_module_separator));

        std::vector<ConfigPart> cfgParts = {globalConfigPart};

        // Add group configs for both generic and specific module names
        appendGroupConfigs(cfgParts, groupAssignments, modName, specificModName);
        appendGroupConfigs(cfgParts, groupAssignments, specificModName, specificModName);

        // Add module-specific configs in precedence order
        appendConfigIfPresent(cfgParts, moduleCfgs, modName);
        appendConfigIfPresent(cfgParts, moduleAttrCfgs, specificModName);
        appendConfigIfPresent(cfgParts, specificModuleCfgs, specificModName);

        // TODO: validate cfg and enforce some values
        finalModuleCfgs[specificModName] = assembleConfig(cfgParts, globalCfg);
    }

    // Also assemble configs for non-submodule modules (e.g., the top-level chip).
    // These have no '$' in their name, so modName == specificModName.
    for (auto module : design->modules()) {
        if (module->has_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE)) {
            continue;
        }

        Yosys::RTLIL::IdString modName = module->name;

        std::vector<ConfigPart> cfgParts = {globalConfigPart};
        appendGroupConfigs(cfgParts, groupAssignments, modName, modName);
        appendConfigIfPresent(cfgParts, moduleCfgs, modName);
        appendConfigIfPresent(cfgParts, moduleAttrCfgs, modName);

        finalModuleCfgs[modName] = assembleConfig(cfgParts, globalCfg);
    }

    loadCustomVoters(design);
    validateCfg(design);
}

const Config *ConfigManager::getConfig(Yosys::RTLIL::Module *mod) const {
    if (finalModuleCfgs.count(mod->name) == 0) {
        return &globalCfg;
    }

    return &finalModuleCfgs.at(mod->name);
}

std::string ConfigManager::getConfigAsString(Yosys::RTLIL::Module *mod) const {
    const Config *c = getConfig(mod);
    std::string ret;

    ret += "TMR-Mode: " + tmrModeToString(c->tmrMode) + "\n";
    ret += "TMR-Voter: " + tmrVoterToString(c->tmrVoter) + "\n";
    if (c->tmrVoter == TmrVoter::Custom) {
        ret += "TMR-Voter File: " + c->tmrVoterFile + "\n";
        ret += "TMR-Voter Module: " + c->tmrVoterModule + "\n";
        if (!c->tmrVoterClockPortName.empty())
            ret += "TMR-Voter Clock Port: " + c->tmrVoterClockPortName + "\n";
        if (!c->tmrVoterResetPortName.empty())
            ret += "TMR-Voter Reset Port: " + c->tmrVoterResetPortName + "\n";
        if (!c->tmrVoterClockNet.empty())
            ret += "TMR-Voter Clock Net: " + c->tmrVoterClockNet + "\n";
        if (!c->tmrVoterResetNet.empty())
            ret += "TMR-Voter Reset Net: " + c->tmrVoterResetNet + "\n";
    }
    ret += "TMR-Voter Safe Mode: " + boolToString(c->tmrVoterSafeMode) + "\n";
    ret += "Preserve Mod Ports: " + boolToString(c->preserveModulePorts) + "\n";
    ret += "Prevent Renaming: " + boolToString(c->preventRenaming) + "\n";
    ret += "Logic.insertVoterBeforeFf: " + boolToString(c->insertVoterBeforeFf) + "\n";
    ret += "Logic.insertVoterAfterFf: " + boolToString(c->insertVoterAfterFf) + "\n";
    ret += "Full Module.insert_voter_before_modules: " +
           boolToString(c->tmrModeFullModuleInsertVoterBeforeModules) + "\n";
    ret += "Full Module.insert_voter_after_modules: " +
           boolToString(c->tmrModeFullModuleInsertVoterAfterModules) + "\n";
    ret += "Full Module.insert_voter_on_clock_nets: " +
           boolToString(c->tmrModeFullModuleInsertVoterOnClockNets) + "\n";
    ret += "Full Module.insert_voter_on_reset_nets: " +
           boolToString(c->tmrModeFullModuleInsertVoterOnResetNets) + "\n";
    ret += "Clock port names: " + poolToString(c->clockPortNames) + "\n";
    ret += "Expand Clock net: " + boolToString(c->expandClock) + "\n";
    ret += "Reset port names: " + poolToString(c->resetPortNames) + "\n";
    ret += "Expand reset net: " + boolToString(c->expandReset) + "\n";
    ret += "FF Cells: " + poolToString(c->ffCells) + "\n";
    ret += "Additional FF Cells: " + poolToString(c->additionalFfCells) + "\n";
    ret += "Excluded FF Cells: " + poolToString(c->excludedFfCells) + "\n";
    ret += "Logic.logicPath1Suffix: " + c->logicPath1Suffix + "\n";
    ret += "Logic.logicPath2Suffix: " + c->logicPath2Suffix + "\n";
    ret += "Logic.logicPath3Suffix: " + c->logicPath3Suffix + "\n";
    if (!c->errorPortName.empty())
        ret += "Error port name: " + c->errorPortName + "\n";
    ret += "Auto error port: " + boolToString(c->autoErrorPort) + "\n";

    return ret;
}

} // namespace TMRX
YOSYS_NAMESPACE_END
