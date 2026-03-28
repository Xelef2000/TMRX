#include "tmrx_logic_expansion.h"
#include "config_manager.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "tmrx_utils.h"
#include "utils.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {
namespace {

using detail::ChildPortNames;
using detail::PortKind;
using detail::PortShape;
using detail::ResolvedSubmodule;
using detail::TriplicatedSignals;

Yosys::pool<RTLIL::SigSpec> clkNetWires;
Yosys::pool<RTLIL::SigSpec> rstNetWires;

bool isInClkNet(const RTLIL::Wire *w) { return clkNetWires.count(RTLIL::SigSpec(w)) != 0; }

bool isInRstNet(const RTLIL::Wire *w) { return rstNetWires.count(RTLIL::SigSpec(w)) != 0; }

RTLIL::IdString getDomainAttributeName(const std::string &suffix) {
    return RTLIL::IdString("\\tmr_domain" + suffix);
}

bool isLogicTreeCell(RTLIL::Cell *cell, RTLIL::Design *design) {
    RTLIL::Module *cellMod = design->module(cell->type);
    return cellMod == nullptr || !isProperSubmodule(cellMod) || cellMod->get_blackbox_attribute();
}

void setCellDomainAttribute(RTLIL::Cell *cell, const std::string &suffix) {
    cell->set_bool_attribute(getDomainAttributeName(suffix), true);
}

bool shouldKeepWireShared(const RTLIL::Wire *wire, const Config *cfg) {
    return isTmrErrorOutWire(const_cast<RTLIL::Wire *>(wire), cfg) ||
           (cfg->preserveModulePorts && wire->port_input) ||
           (isClkWire(wire, cfg) && !cfg->expandClock) ||
           (isRstWire(wire, cfg) && !cfg->expandReset);
}

bool isExposedModulePort(const RTLIL::Wire *wire) {
    return wire != nullptr && (wire->port_input || wire->port_output);
}

TriplicatedSignals deriveParentSignals(
    const RTLIL::SigSpec &signalA,
    const dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> &wireMap) {
    TriplicatedSignals signals = {signalA, signalA, signalA};

    for (auto &kv : wireMap) {
        signals.signalB.replace(kv.first, kv.second.first);
        signals.signalC.replace(kv.first, kv.second.second);
    }

    return signals;
}

bool parentSignalsAreShared(const TriplicatedSignals &signals) {
    return signals.signalA == signals.signalB && signals.signalA == signals.signalC;
}

PortKind classifyPortKind(const RTLIL::Wire *portWire, const Config *childCfg) {
    if (isTmrErrorOutWire(const_cast<RTLIL::Wire *>(portWire), childCfg)) {
        return PortKind::Error;
    }
    if (isClkWire(portWire, childCfg)) {
        return PortKind::Clock;
    }
    if (isRstWire(portWire, childCfg)) {
        return PortKind::Reset;
    }
    return PortKind::Data;
}

ChildPortNames resolveChildPortNames(RTLIL::Module *effectiveCellMod, RTLIL::IdString logicalPort,
                                     const Config *childCfg) {
    RTLIL::IdString portA = RTLIL::IdString(logicalPort.str() + childCfg->logicPath1Suffix);
    RTLIL::IdString portB = RTLIL::IdString(logicalPort.str() + childCfg->logicPath2Suffix);
    RTLIL::IdString portC = RTLIL::IdString(logicalPort.str() + childCfg->logicPath3Suffix);

    bool hasBase = isExposedModulePort(effectiveCellMod->wire(logicalPort));
    bool hasA = isExposedModulePort(effectiveCellMod->wire(portA));
    bool hasB = isExposedModulePort(effectiveCellMod->wire(portB));
    bool hasC = isExposedModulePort(effectiveCellMod->wire(portC));

    if (hasBase && !hasA && !hasB && !hasC) {
        return {PortShape::Shared, logicalPort, logicalPort, logicalPort};
    }

    if (!hasBase && hasA && hasB && hasC) {
        return {PortShape::Triplicated, portA, portB, portC};
    }

    if (!hasBase && !hasA && !hasB && !hasC) {
        log_error("Submodule port '%s' not found on effective module '%s'.\n", logicalPort.c_str(),
                  effectiveCellMod->name.c_str());
    }

    log_error("Unexpected mixed port expansion for '%s' on module '%s'.\n", logicalPort.c_str(),
              effectiveCellMod->name.c_str());
}

void connectParentDestinations(RTLIL::Module *mod, const TriplicatedSignals &destinations,
                               const RTLIL::SigSpec &source) {
    mod->connect(destinations.signalA, source);
    if (destinations.signalB != destinations.signalA) {
        mod->connect(destinations.signalB, source);
    }
    if (destinations.signalC != destinations.signalA &&
        destinations.signalC != destinations.signalB) {
        mod->connect(destinations.signalC, source);
    }
}

ResolvedSubmodule resolveSubmodule(RTLIL::Module *mod, RTLIL::Cell *cell,
                                   const ConfigManager *cfgMgr) {
    RTLIL::Module *logicalModule = mod->design->module(cell->type);
    if (logicalModule == nullptr) {
        log_error("No submodule definition found for cell '%s' (type '%s').\n", cell->name.c_str(),
                  cell->type.c_str());
    }

    const Config *childCfg = cfgMgr->getConfig(logicalModule);
    RTLIL::Module *effectiveModule = logicalModule;
    bool wasExpandedWithTriplicatedPorts = logicalModule->has_attribute(ATTRIBUTE_IMPL_MODULE);

    if (wasExpandedWithTriplicatedPorts) {
        RTLIL::IdString implName =
            RTLIL::IdString(logicalModule->get_string_attribute(ATTRIBUTE_IMPL_MODULE));
        cell->type = implName;
        effectiveModule = mod->design->module(implName);
        if (effectiveModule == nullptr) {
            log_error("No remapped submodule implementation '%s' for cell '%s'.\n",
                      implName.c_str(), cell->name.c_str());
        }
    }

    return {logicalModule, effectiveModule, childCfg, wasExpandedWithTriplicatedPorts};
}

void buildClkNet(RTLIL::Module *mod, const ConfigManager *cfgMgr) {
    clkNetWires.clear();

    const Config *cfg = cfgMgr->getConfig(mod);
    RTLIL::Design *design = mod->design;

    // Add wires connected to clock ports of the current module
    for (auto wire : mod->wires()) {
        if ((wire->port_input || wire->port_output) && isClkWire(wire, cfg)) {
            clkNetWires.insert(RTLIL::SigSpec(wire));
        }
    }

    // Add wires connected to clock ports of cells (submodules)
    for (auto cell : mod->cells()) {
        RTLIL::Module *cellMod = design->module(cell->type);
        if (cellMod == nullptr || !isProperSubmodule(cellMod)) {
            continue;
        }

        const Config *cellCfg = cfgMgr->getConfig(cellMod);

        for (auto &conn : cell->connections()) {
            RTLIL::IdString portName = conn.first;
            RTLIL::Wire *portWire = cellMod->wire(portName);

            if (portWire != nullptr && isClkWire(portWire, cellCfg)) {
                clkNetWires.insert(conn.second);
            }
        }
    }
}

void buildRstNet(RTLIL::Module *mod, const ConfigManager *cfgMgr) {
    rstNetWires.clear();

    const Config *cfg = cfgMgr->getConfig(mod);
    RTLIL::Design *design = mod->design;

    // Add wires connected to reset ports of the current module
    for (auto wire : mod->wires()) {
        if ((wire->port_input || wire->port_output) && isRstWire(wire, cfg)) {
            rstNetWires.insert(RTLIL::SigSpec(wire));
        }
    }

    // Add wires connected to reset ports of cells (submodules)
    for (auto cell : mod->cells()) {
        RTLIL::Module *cellMod = design->module(cell->type);
        if (cellMod == nullptr || !isProperSubmodule(cellMod)) {
            continue;
        }

        const Config *cellCfg = cfgMgr->getConfig(cellMod);

        for (auto &conn : cell->connections()) {
            RTLIL::IdString portName = conn.first;
            RTLIL::Wire *portWire = cellMod->wire(portName);

            if (portWire != nullptr && isRstWire(portWire, cellCfg)) {
                rstNetWires.insert(conn.second);
            }
        }
    }
}

std::vector<RTLIL::Wire *> connectSubmodulePorts(
    RTLIL::Module *mod, RTLIL::Cell *cell, RTLIL::Module *logicalCellMod,
    RTLIL::Module *effectiveCellMod, const Config *childCfg, const Config *parentCfg,
    const dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> &wireMap) {
    std::vector<RTLIL::Wire *> errorSignals;
    if (!isProperSubmodule(logicalCellMod)) {
        return errorSignals;
    }

    dict<RTLIL::IdString, RTLIL::SigSpec> origConnections;
    for (auto &conn : cell->connections()) {
        origConnections[conn.first] = conn.second;
    }

    cell->connections_.clear();

    for (auto &port : effectiveCellMod->ports) {
        RTLIL::Wire *portWire = effectiveCellMod->wire(port);
        if (origConnections.count(port) == 0 && isTmrErrorOutWire(portWire, childCfg)) {
            RTLIL::Wire *err_wire = mod->addWire(NEW_ID, portWire->width);
            cell->setPort(port, err_wire);
            errorSignals.push_back(err_wire);
        }
    }

    for (auto &origConn : origConnections) {
        RTLIL::IdString logicalPort = origConn.first;
        RTLIL::Wire *logicalPortWire = logicalCellMod->wire(logicalPort);
        if (logicalPortWire == nullptr) {
            log_error("No logical port '%s' on submodule '%s'.\n", logicalPort.c_str(),
                      logicalCellMod->name.c_str());
        }

        PortKind portKind = classifyPortKind(logicalPortWire, childCfg);
        ChildPortNames childPorts = resolveChildPortNames(effectiveCellMod, logicalPort, childCfg);
        TriplicatedSignals parentSignals = deriveParentSignals(origConn.second, wireMap);
        bool parentShared = parentSignalsAreShared(parentSignals);

        if (logicalPortWire->port_input && logicalPortWire->port_output) {
            log_error("Inout port '%s' on submodule '%s' is not supported.\n", logicalPort.c_str(),
                      logicalCellMod->name.c_str());
        }

        if (logicalPortWire->port_input) {
            if (childPorts.shape == PortShape::Shared) {
                if (parentShared) {
                    cell->setPort(childPorts.portA, parentSignals.signalA);
                } else {
                    auto [votedSignal, errorSignal] = insertVoter(
                        mod, {parentSignals.signalA, parentSignals.signalB, parentSignals.signalC},
                        parentCfg);
                    errorSignals.push_back(errorSignal);
                    cell->setPort(childPorts.portA, votedSignal);
                }
            } else {
                if (parentShared) {
                    cell->setPort(childPorts.portA, parentSignals.signalA);
                    cell->setPort(childPorts.portB, parentSignals.signalA);
                    cell->setPort(childPorts.portC, parentSignals.signalA);
                } else {
                    cell->setPort(childPorts.portA, parentSignals.signalA);
                    cell->setPort(childPorts.portB, parentSignals.signalB);
                    cell->setPort(childPorts.portC, parentSignals.signalC);
                }
            }
            continue;
        }

        if (!logicalPortWire->port_output) {
            if (portKind == PortKind::Error) {
                continue;
            }
            log_error("Port '%s' on submodule '%s' is neither input nor output.\n",
                      logicalPort.c_str(), logicalCellMod->name.c_str());
        }

        if (childPorts.shape == PortShape::Shared) {
            RTLIL::Wire *newOutput = mod->addWire(NEW_ID, origConn.second.size());
            cell->setPort(childPorts.portA, newOutput);
            connectParentDestinations(mod, parentSignals, newOutput);
            continue;
        }

        if (!parentShared) {
            cell->setPort(childPorts.portA, parentSignals.signalA);
            cell->setPort(childPorts.portB, parentSignals.signalB);
            cell->setPort(childPorts.portC, parentSignals.signalC);
            continue;
        }

        RTLIL::Wire *outA = mod->addWire(NEW_ID, origConn.second.size());
        RTLIL::Wire *outB = mod->addWire(NEW_ID, origConn.second.size());
        RTLIL::Wire *outC = mod->addWire(NEW_ID, origConn.second.size());
        cell->setPort(childPorts.portA, outA);
        cell->setPort(childPorts.portB, outB);
        cell->setPort(childPorts.portC, outC);

        auto [votedSignal, errorSignal] = insertVoter(mod, {outA, outB, outC}, parentCfg);
        errorSignals.push_back(errorSignal);
        connectParentDestinations(mod, parentSignals, votedSignal);
    }

    return errorSignals;
}

std::vector<RTLIL::Wire *> insertVoterAfterFf(RTLIL::Module *mod,
                                              dict<Cell *, std::pair<Cell *, Cell *>> ffMap,
                                              const Config *cfg) {
    std::vector<RTLIL::Wire *> errorSignals;

    for (auto flipFlops : ffMap) {

        auto [input_ports, output_ports] = getPortNames(flipFlops.first, mod->design);

        if (output_ports.empty()) {
            log("Cell Type: %s\n", flipFlops.first->type.str().c_str());
            log_error("Flip Flop without output found");
        }

        for (auto port : output_ports) {
            std::vector<RTLIL::SigSpec> intermediateWires;
            std::vector<RTLIL::SigSpec> originalSignals;

        for (auto ff : {flipFlops.first, flipFlops.second.first, flipFlops.second.second}) {
                RTLIL::SigSpec out_signal = ff->getPort(port);
                RTLIL::Wire *intermediate_wire = mod->addWire(NEW_ID, out_signal.size());

                ff->setPort(port, intermediate_wire);
                intermediateWires.push_back(intermediate_wire);
                originalSignals.push_back(out_signal);
            }

            for (size_t i = 0; i < tmrx_replication_factor; i++) {
                std::pair<RTLIL::Wire *, RTLIL::Wire *> resultWires =
                    insertVoter(mod, intermediateWires, cfg,
                                i == 0 ? cfg->logicPath1Suffix
                                       : (i == 1 ? cfg->logicPath2Suffix
                                                 : cfg->logicPath3Suffix));
                mod->connect(originalSignals.at(i), resultWires.first);

                errorSignals.push_back(resultWires.second);
            }
        }
    }

    return errorSignals;
}

std::vector<RTLIL::Wire *>
insertOutputVoters(RTLIL::Module *mod,
                   dict<RTLIL::Wire *, std::pair<RTLIL::Wire *, RTLIL::Wire *>> outputMap,
                   const Config *cfg) {
    std::vector<RTLIL::Wire *> errorSignals;
    for (auto outputs : outputMap) {
        if (!cfg->preserveModulePorts && isInClkNet(outputs.first) && cfg->expandClock)
            continue;
        if (!cfg->preserveModulePorts && isInRstNet(outputs.first) && cfg->expandReset)
            continue;
        if (!cfg->preserveModulePorts && !isInClkNet(outputs.first) && !isInRstNet(outputs.first))
            continue;

        outputs.first->port_output = false;

        std::vector<RTLIL::SigSpec> outputSignals = {};

        outputs.first->port_output = false;
        outputSignals.push_back(outputs.first);

        outputs.second.first->port_output = false;
        outputSignals.push_back(outputs.second.first);

        outputs.second.second->port_output = false;
        outputSignals.push_back(outputs.second.second);

        // if(outputSignals.at(0) == outputSignals.at(1) && outputSignals.at(0) ==
        // outputSignals.at(2)) continue;

        std::pair<RTLIL::Wire *, RTLIL::Wire *> resultWires = insertVoter(mod, outputSignals, cfg);
        errorSignals.push_back(resultWires.second);

        resultWires.first->port_output = true;
        mod->rename(resultWires.first,
                    mod->uniquify(outputs.first->name.str().substr(
                        0, outputs.first->name.str().size() - (cfg->logicPath1Suffix.size()))));
    }

    return errorSignals;
}

void renameWiresAndCells(RTLIL::Module *mod, std::vector<RTLIL::Wire *> wires,
                         std::vector<RTLIL::Cell *> cells, std::string suffix, const Config *cfg) {
    for (auto w : wires) {
        if (shouldKeepWireShared(w, cfg)) {
            continue;
        }
        mod->rename(w, mod->uniquify(w->name.str() + suffix));
    }

    for (auto c : cells) {
        if (!isLogicTreeCell(c, c->module->design)) {
            continue;
        }

        setCellDomainAttribute(c, suffix);
        mod->rename(c, mod->uniquify(c->name.str() + suffix));
    }
}

std::tuple<dict<RTLIL::SigSpec, RTLIL::SigSpec>, dict<RTLIL::Wire *, RTLIL::Wire *>,
           dict<RTLIL::Cell *, RTLIL::Cell *>>
insertDuplicateLogic(RTLIL::Module *mod, std::vector<RTLIL::Wire *> wires,
                     std::vector<RTLIL::Cell *> cells, std::vector<RTLIL::SigSig> connections,
                     std::string suffix, const Config *cfg) {
    dict<RTLIL::SigSpec, RTLIL::SigSpec> wireMap;
    dict<RTLIL::Wire *, RTLIL::Wire *> outputMap;
    dict<RTLIL::Cell *, RTLIL::Cell *> flipFlopMap;

    for (auto w : wires) {
        if (shouldKeepWireShared(w, cfg)) {
            wireMap[w] = w;
            continue;
        }

        RTLIL::Wire *w_b = mod->addWire(mod->uniquify(w->name.str() + suffix), w->width);
        w_b->port_input = w->port_input;
        w_b->port_output = w->port_output;
        w_b->start_offset = w->start_offset;
        w_b->upto = w->upto;
        w_b->attributes = w->attributes;

        wireMap[w] = w_b;

        if (w->port_output) {
            outputMap[w] = w_b;
        }
    }

    for (auto c : cells) {
        if (!isLogicTreeCell(c, c->module->design)) {
            continue;
        }

        RTLIL::Cell *c_b = mod->addCell(mod->uniquify(c->name.str() + suffix), c->type);
        setCellDomainAttribute(c_b, suffix);

        // log("Looking at cell %u\n",
        // (isFlipFlop(c, worker, &known_ff_cell_names)));

        if (isFlipFlop(c, mod, cfg)) {
            // TODO: fix this
            flipFlopMap[c] = c_b;
        }

        c_b->parameters = c->parameters;
        c_b->attributes = c->attributes;

        for (auto &connection : c->connections()) {
            RTLIL::SigSpec sig = connection.second;

            for (auto &it : wireMap) {
                sig.replace(it.first, it.second);
            }

            c_b->setPort(connection.first, sig);
        }
    }

    // TODO: imprive this
    for (auto conn : connections) {
        RTLIL::SigSpec first = conn.first;
        RTLIL::SigSpec second = conn.second;

        for (auto &w : wireMap) {
            first.replace(w.first, w.second);
            second.replace(w.first, w.second);
        }

        mod->connect(first, second);
    }

    return {wireMap, outputMap, flipFlopMap};
}

} // namespace
void logicTmrExpansion(RTLIL::Module *mod, const ConfigManager *cfgMgr, const Config *cfgOverride) {
    const Config *cfg = cfgOverride ? cfgOverride : cfgMgr->getConfig(mod);

    std::vector<RTLIL::Wire *> originalWires(mod->wires().begin(), mod->wires().end());
    std::vector<RTLIL::Cell *> originalCells(mod->cells().begin(), mod->cells().end());
    std::vector<RTLIL::SigSig> originalConnections(mod->connections().begin(),
                                                   mod->connections().end());
    std::vector<RTLIL::Wire *> errorWires;

    log("  Logic TMR: expanding '%s' (%zu wire(s), %zu cell(s))\n", mod->name.c_str(),
        originalWires.size(), originalCells.size());

    log("  [1/6] Building clock/reset nets\n");
    buildClkNet(mod, cfgMgr);
    buildRstNet(mod, cfgMgr);

    log("  [2/6] Duplicating logic (paths B and C)\n");
    auto [wireMapB, outputMapB, flipFlopMapB] = insertDuplicateLogic(
        mod, originalWires, originalCells, originalConnections, cfg->logicPath2Suffix, cfg);
    auto [wireMapC, outputMapC, flipFlopMapC] = insertDuplicateLogic(
        mod, originalWires, originalCells, originalConnections, cfg->logicPath3Suffix, cfg);

    buildClkNet(mod, cfgMgr);
    buildRstNet(mod, cfgMgr);

    dict<RTLIL::Wire *, std::pair<RTLIL::Wire *, RTLIL::Wire *>> combinedOutputMap =
        zipDicts(outputMapB, outputMapC);
    dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> combinedWireMap =
        zipDicts(wireMapB, wireMapC);
    dict<RTLIL::Cell *, std::pair<RTLIL::Cell *, RTLIL::Cell *>> combinedFfMap =
        zipDicts(flipFlopMapB, flipFlopMapC);

    log("  [3/6] Connecting submodule ports\n");
    for (auto cell : originalCells) {
        RTLIL::Module *cellMod = mod->design->module(cell->type);
        // Blackbox cells (standard cells, liberty cells) are duplicated like
        // primitive cells in insertDuplicateLogic; they must not be treated
        // as proper submodules here or voters get inserted on every port.
        if (!isProperSubmodule(cellMod) || cellMod->get_blackbox_attribute()) {
            continue;
        }

        ResolvedSubmodule submodule = resolveSubmodule(mod, cell, cfgMgr);

        log("    Connecting submodule '%s' (type '%s', preserve_ports=%s, expanded=%s)\n",
            cell->name.c_str(), cell->type.c_str(),
            submodule.childCfg->preserveModulePorts ? "true" : "false",
            submodule.wasExpandedWithTriplicatedPorts ? "true" : "false");

        auto voterErrorWires =
            connectSubmodulePorts(mod, cell, submodule.logicalModule, submodule.effectiveModule,
                                  submodule.childCfg, cfg, combinedWireMap);
        errorWires.insert(errorWires.end(), voterErrorWires.begin(), voterErrorWires.end());
    }

    log("  [4/6] Renaming path-A wires/cells\n");
    renameWiresAndCells(mod, originalWires, originalCells, cfg->logicPath1Suffix, cfg);

    if (cfg->insertVoterBeforeFf) {
        log_error("Insert before ff not yet implemented");
    }

    if (cfg->insertVoterAfterFf) {
        log("  [5/6] Inserting voters after %zu flip-flop(s)\n", combinedFfMap.size());
        auto voterErrorWires = insertVoterAfterFf(mod, combinedFfMap, cfg);
        errorWires.insert(errorWires.end(), voterErrorWires.begin(), voterErrorWires.end());
    }

    buildClkNet(mod, cfgMgr);
    buildRstNet(mod, cfgMgr);

    log("  [6/6] Inserting output voters / connecting error signal\n");
    if (cfg->preserveModulePorts || !cfg->expandClock || !cfg->expandReset) {
        auto voterErrorWires = insertOutputVoters(mod, combinedOutputMap, cfg);
        errorWires.insert(errorWires.end(), voterErrorWires.begin(), voterErrorWires.end());
    }

    connectErrorSignal(mod, errorWires, cfg);
}

} // namespace TMRX

YOSYS_NAMESPACE_END
