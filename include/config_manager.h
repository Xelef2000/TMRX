#ifndef TMRX_CONFIG_MANAGER_H
#define TMRX_CONFIG_MANAGER_H

#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include "tmrx_constants.h"
#include "toml11/toml.hpp"
#include <optional>
#include <string>
#include <vector>

YOSYS_NAMESPACE_BEGIN
namespace TMRX {

enum class TmrMode {
    None,
    FullModuleTMR,
    LogicTMR,
};

enum class TmrVoter { Default, Custom };

// String conversion helpers
std::optional<TmrMode> parseTmrMode(const std::string &str);
std::string tmrModeToString(TmrMode mode);
std::optional<TmrVoter> parseTmrVoter(const std::string &str);
std::string tmrVoterToString(TmrVoter voter);

// TOML parsing helpers
template <typename T>
std::optional<T> tomlFindOptional(const toml::value &t, const std::string &key);

std::optional<Yosys::pool<Yosys::RTLIL::IdString>> tomlParseIdStringPool(const toml::value &t,
                                                                         const std::string &key);

// Config assembly helper
template <typename T> void applyIfPresent(T &dest, const std::optional<T> &src);

// String formatting helpers
std::string poolToString(const Yosys::pool<Yosys::RTLIL::IdString> &pool);
std::string boolToString(bool val);

struct Config {

    TmrMode tmrMode;

    TmrVoter tmrVoter;
    std::string tmrVoterFile;
    std::string tmrVoterModule;
    std::string tmrVoterClockPortName; // voter's clock input port name
    std::string tmrVoterResetPortName; // voter's reset input port name
    std::string tmrVoterClockNet; // parent wire to drive voter clock (default: first clock port)
    std::string tmrVoterResetNet; // parent wire to drive voter reset (default: first reset port)

    bool tmrVoterSafeMode;

    bool preserveModulePorts;
    bool preventRenaming;

    bool insertVoterBeforeFf;
    bool insertVoterAfterFf;

    bool tmrModeFullModuleInsertVoterBeforeModules;
    bool tmrModeFullModuleInsertVoterAfterModules;
    bool tmrModeFullModuleInsertVoterOnClockNets;
    bool tmrModeFullModuleInsertVoterOnResetNets;

    Yosys::pool<Yosys::RTLIL::IdString> clockPortNames;
    bool expandClock;

    Yosys::pool<Yosys::RTLIL::IdString> resetPortNames;
    bool expandReset;

    Yosys::pool<Yosys::RTLIL::IdString> ffCells;
    Yosys::pool<Yosys::RTLIL::IdString> additionalFfCells;
    Yosys::pool<Yosys::RTLIL::IdString> excludedFfCells;

    std::string logicPath1Suffix;
    std::string logicPath2Suffix;
    std::string logicPath3Suffix;

    // Name of the output port that collects aggregated voter error signals.
    // Overrides (or supplements) the per-wire `(* tmrx_error_sink *)` attribute.
    // Empty string means "use attribute only".
    std::string errorPortName;

    // When true and the module has no error sink, automatically create a new
    // 1-bit output port named `tmrx_err_o` (uniquified to avoid collisions)
    // and connect the aggregated voter error signals to it.
    bool autoErrorPort;
};

struct ConfigPart {

    std::optional<TmrMode> tmrMode;

    std::optional<TmrVoter> tmrVoter;
    std::optional<std::string> tmrVoterFile;
    std::optional<std::string> tmrVoterModule;
    std::optional<std::string> tmrVoterClockPortName;
    std::optional<std::string> tmrVoterResetPortName;
    std::optional<std::string> tmrVoterClockNet;
    std::optional<std::string> tmrVoterResetNet;
    std::optional<bool> tmrVoterSafeMode;

    std::optional<bool> preserveModulePorts;
    std::optional<bool> preventRenaming;

    std::optional<bool> insertVoterBeforeFf;
    std::optional<bool> insertVoterAfterFf;

    std::optional<bool> tmrModeFullModuleInsertVoterBeforeModules;
    std::optional<bool> tmrModeFullModuleInsertVoterAfterModules;
    std::optional<bool> tmrModeFullModuleInsertVoterOnClockNets;
    std::optional<bool> tmrModeFullModuleInsertVoterOnResetNets;

    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> clockPortNames;
    std::optional<bool> expandClock;

    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> resetPortNames;
    std::optional<bool> expandReset;

    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> ffCells;
    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> additionalFfCells;
    std::optional<Yosys::pool<Yosys::RTLIL::IdString>> excludedFfCells;

    std::optional<std::string> logicPath1Suffix;
    std::optional<std::string> logicPath2Suffix;
    std::optional<std::string> logicPath3Suffix;

    std::optional<std::string> errorPortName;
    std::optional<bool> autoErrorPort;
};

struct ConfigManager {
  private:
    void loadGlobalDefaultCfg();
    void loadDefaultGroupsCfg();
    void loadCustomVoters(Yosys::RTLIL::Design *design);
    void validateCfg(Yosys::RTLIL::Design *design);

    // Attribute parsing helpers
    std::string getStringAttrValueOr(const Yosys::RTLIL::Module *mod, const std::string &attr,
                                     const std::string &def);
    bool getBoolAttrValueOr(const Yosys::RTLIL::Module *mod, const std::string &attr, bool def);
    int getIntAttrValueOr(const Yosys::RTLIL::Module *mod, const std::string &attr, int def);
    std::optional<std::string> getStringAttrValue(const Yosys::RTLIL::Module *mod,
                                                  const std::string &attr);
    std::optional<bool> getBoolAttrValue(const Yosys::RTLIL::Module *mod, const std::string &attr);
    std::optional<int> getIntAttrValue(const Yosys::RTLIL::Module *mod, const std::string &attr);
    std::optional<std::vector<std::string>> getStringListAttrValue(const Yosys::RTLIL::Module *mod,
                                                                   const std::string &attr);
    std::optional<Yosys::pool<Yosys::RTLIL::IdString>>
    parseAttrIdStringPool(const Yosys::RTLIL::Module *mod, const std::string &attr);

    // Config parsing and assembly
    ConfigPart parseConfig(const toml::value &t);
    ConfigPart parseModuleAnnotations(const Yosys::RTLIL::Module *mod);
    Config assembleConfig(std::vector<ConfigPart> parts, Config def);

    // Config assembly helpers
    void appendGroupConfigs(
        std::vector<ConfigPart> &cfgParts,
        const Yosys::dict<Yosys::RTLIL::IdString, std::vector<std::string>> &groupAssignments,
        Yosys::RTLIL::IdString lookupName, Yosys::RTLIL::IdString logName);
    void appendConfigIfPresent(std::vector<ConfigPart> &cfgParts,
                               const Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> &cfgMap,
                               Yosys::RTLIL::IdString key);

    Config globalCfg;

    Yosys::dict<std::string, ConfigPart> groupCfg;
    Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> moduleCfgs;
    Yosys::dict<Yosys::RTLIL::IdString, Config> finalModuleCfgs;
    Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> specificModuleCfgs;
    Yosys::dict<Yosys::RTLIL::IdString, ConfigPart> moduleAttrCfgs;

  public:
    ConfigManager(Yosys::RTLIL::Design *design, const std::string &configFile);
    const Config *getConfig(Yosys::RTLIL::Module *mod) const;
    std::string getConfigAsString(Yosys::RTLIL::Module *mod) const;
};

} // namespace TMRX
YOSYS_NAMESPACE_END

#endif
