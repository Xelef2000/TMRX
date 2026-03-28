#ifndef TMRX_LOGIC_EXPANSION_H
#define TMRX_LOGIC_EXPANSION_H

#include "config_manager.h"
#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {

namespace detail {

enum class PortKind {
    Data,
    Clock,
    Reset,
    Error,
};

enum class PortShape {
    Shared,
    Triplicated,
};

struct TriplicatedSignals {
    RTLIL::SigSpec signalA;
    RTLIL::SigSpec signalB;
    RTLIL::SigSpec signalC;
};

struct ChildPortNames {
    PortShape shape;
    RTLIL::IdString portA;
    RTLIL::IdString portB;
    RTLIL::IdString portC;
};

struct ResolvedSubmodule {
    RTLIL::Module *logicalModule;
    RTLIL::Module *effectiveModule;
    const Config *childCfg;
    bool wasExpandedWithTriplicatedPorts;
};

} // namespace detail

void logicTmrExpansion(RTLIL::Module *mod, const ConfigManager *cfgMgr,
                       const Config *cfgOverride = nullptr);

} // namespace TMRX
YOSYS_NAMESPACE_END

#endif
