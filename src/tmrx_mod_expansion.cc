#include "tmrx_mod_expansion.h"
#include "kernel/rtlil.h"
#include "tmrx_utils.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {
namespace {

RTLIL::IdString getDomainAttributeName(const std::string &suffix) {
    return RTLIL::IdString("\\tmr_domain" + suffix);
}

void setCellDomainAttribute(RTLIL::Cell *cell, const std::string &suffix) {
    cell->set_bool_attribute(getDomainAttributeName(suffix), true);
}

bool isRecursiveDomainAttrExempt(RTLIL::Module *mod, const ConfigManager *cfgMgr) {
    if (mod == nullptr) {
        return false;
    }

    const Config *cfg = cfgMgr->getConfig(mod);
    return cfg->preventRenaming || (cfg->tmrMode == TmrMode::LogicTMR && cfg->preserveModulePorts);
}

// Recursively clone and rename all proper submodule cells within `mod`,
// appending `suffix` to their type names.  Creates new uniquified module
// definitions in `design` as needed (skips creation if already present).
void uniquifySubmodulesRecursive(RTLIL::Module *mod, const std::string &suffix,
                                 RTLIL::Design *design, const ConfigManager *cfgMgr) {
    std::vector<RTLIL::Cell *> cells(mod->cells().begin(), mod->cells().end());

    for (auto cell : cells) {
        RTLIL::Module *cellMod = design->module(cell->type);
        if (cellMod == nullptr || !isProperSubmodule(cellMod)) {
            setCellDomainAttribute(cell, suffix);
            continue;
        }

        if (isRecursiveDomainAttrExempt(cellMod, cfgMgr)) {
            continue;
        }

        setCellDomainAttribute(cell, suffix);
        // Blackbox modules (standard cells, user IP blackboxes) are
        // independent per-instance by definition — multiple workers can share
        // the same blackbox type without needing uniquified copies.
        // Cloning them also risks breaking port connections when the original
        // was already processed by fullModuleTmrExpansion.
        if (cellMod->get_blackbox_attribute()) {
            continue;
        }

        RTLIL::IdString uniqueName = RTLIL::IdString(cell->type.str() + suffix);

        if (design->module(uniqueName) == nullptr) {
            RTLIL::Module *uniqueMod = design->addModule(uniqueName);
            cellMod->cloneInto(uniqueMod);
            uniqueMod->name = uniqueName;
            uniqueMod->set_bool_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE, true);
            // cloneInto copies all attributes, including tmrx_impl_module if
            // the source was processed by TMRX. The clone is an independent
            // worker module — it must not carry tmrx_impl_module or the
            // cleanup loop in tmrx_pass will remove it from the design.
            uniqueMod->attributes.erase(ATTRIBUTE_IMPL_MODULE);

            // Recurse so deeper submodule levels are also uniquified.
            uniquifySubmodulesRecursive(uniqueMod, suffix, design, cfgMgr);
        }

        cell->type = uniqueName;
    }
}

} // namespace

void fullModuleTmrExpansion(RTLIL::Module *mod, const ConfigManager *cfgMgr, const Config *cfg) {
    std::vector<RTLIL::Wire *> errorWires;

    RTLIL::IdString originalModuleName = mod->name;
    log("  Full Module TMR: creating %zu-instance wrapper for '%s'\n", tmrx_replication_factor,
        originalModuleName.c_str());
    std::string moduleName = mod->name.str() + tmrx_worker_module_suffix;

    mod->design->rename(mod, moduleName);
    RTLIL::Module *wrapper = mod->design->addModule(originalModuleName);
    wrapper->set_bool_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE);
    dict<RTLIL::Wire *, std::vector<RTLIL::Wire *>> wireMap;

    std::vector<std::string> suffixes =
        (cfg->preserveModulePorts
             ? std::vector<std::string>{""}
             : std::vector<std::string>{cfg->logicPath1Suffix, cfg->logicPath2Suffix,
                                        cfg->logicPath3Suffix});

    for (size_t i = 0; i < tmrx_replication_factor; i++) {
        for (auto w : mod->wires()) {
            if (w->port_id == 0) {
                continue;
            }
            bool ignoreWire = (isClkWire(w, cfg) && !cfg->expandClock) ||
                              (isRstWire(w, cfg) && !cfg->expandReset) || isTmrErrorOutWire(w, cfg);

            if ((cfg->preserveModulePorts || ignoreWire) && i > 0) {
                wireMap[w].push_back(wireMap.at(w).at(0));
                continue;
            }

            std::string newWireName = (w->name.str() + (ignoreWire ? "" : suffixes.at(i)));

            RTLIL::Wire *w_b = wrapper->addWire(newWireName, w->width);
            w_b->port_input = w->port_input;
            w_b->port_output = w->port_output;
            w_b->start_offset = w->start_offset;
            w_b->upto = w->upto;
            w_b->attributes = w->attributes;
            wireMap[w].push_back(w_b);
        }
    }
    wrapper->fixup_ports();

    std::vector<RTLIL::Cell *> duplicates;
    std::vector<dict<RTLIL::Wire *, RTLIL::Wire *>> cellPorts;

    for (size_t i = 0; i < tmrx_replication_factor; i++) {
        RTLIL::Cell *cell = wrapper->addCell(NEW_ID, moduleName);
        duplicates.push_back(cell);

        cellPorts.push_back({});

        for (auto w : mod->wires()) {
            if (w->port_id == 0) {
                continue;
            }
            RTLIL::Wire *w_con = wrapper->addWire(NEW_ID, w->width);
            cellPorts.at(i)[(w)] = w_con;
            cell->setPort(w->name, w_con);

            if (isTmrErrorOutWire(w, cfg)) {
                errorWires.push_back(w_con);
            }
        }
    }

    if (cfg->tmrModeFullModuleInsertVoterBeforeModules && !cfg->preserveModulePorts) {
        for (auto wm : wireMap) {
            if (!wm.first->port_input || (isClkWire(wm.first, cfg) && !cfg->expandClock) ||
                (isRstWire(wm.first, cfg) && !cfg->expandReset) ||
                (isClkWire(wm.first, cfg) && !cfg->tmrModeFullModuleInsertVoterOnClockNets) ||
                (isRstWire(wm.first, cfg) && !cfg->tmrModeFullModuleInsertVoterOnResetNets)) {
                continue;
            }

            std::vector<RTLIL::Wire *> voterOutputs;

            for (size_t i = 0; i < tmrx_replication_factor; i++) {
                auto [voterOutput, err] =
                    insertVoter(wrapper, {wm.second.at(0), wm.second.at(1), wm.second.at(2)}, cfg,
                                suffixes.at(i));
                errorWires.push_back(err);
                voterOutputs.push_back(voterOutput);
            }

            wireMap[wm.first] = voterOutputs;
        }
    }

    if (cfg->tmrModeFullModuleInsertVoterAfterModules && !cfg->preserveModulePorts) {
        for (auto wm : wireMap) {
            if (!wm.first->port_output || isTmrErrorOutWire(wm.first, cfg) ||
                (isClkWire(wm.first, cfg) && !cfg->expandClock) ||
                (isRstWire(wm.first, cfg) && !cfg->expandReset) ||
                (isClkWire(wm.first, cfg) && !cfg->tmrModeFullModuleInsertVoterOnClockNets) ||
                (isRstWire(wm.first, cfg) && !cfg->tmrModeFullModuleInsertVoterOnResetNets)) {
                continue;
            }

            std::vector<RTLIL::Wire *> voterOutputs;

            for (size_t i = 0; i < tmrx_replication_factor; i++) {
                auto [voterOutput, err] =
                    insertVoter(wrapper,
                                {cellPorts.at(0).at(wm.first), cellPorts.at(1).at(wm.first),
                                 cellPorts.at(2).at(wm.first)},
                                cfg, suffixes.at(i));
                errorWires.push_back(err);
                voterOutputs.push_back(voterOutput);
            }

            cellPorts.at(0).at(wm.first) = voterOutputs.at(0);
            cellPorts.at(1).at(wm.first) = voterOutputs.at(1);
            cellPorts.at(2).at(wm.first) = voterOutputs.at(2);
        }
    }

    if (cfg->preserveModulePorts || !cfg->expandClock || !cfg->expandReset) {
        for (auto wm : wireMap) {
            if (!wm.first->port_output)
                continue;
            if (isTmrErrorOutWire(wm.first, cfg))
                continue;
            if (!cfg->preserveModulePorts && isClkWire(wm.first, cfg) && cfg->expandClock)
                continue;
            if (!cfg->preserveModulePorts && isRstWire(wm.first, cfg) && cfg->expandReset)
                continue;
            if (!cfg->preserveModulePorts && !isClkWire(wm.first, cfg) && !isRstWire(wm.first, cfg))
                continue;

            auto [voterOutput, err] =
                insertVoter(wrapper,
                            {cellPorts.at(0).at(wm.first), cellPorts.at(1).at(wm.first),
                             cellPorts.at(2).at(wm.first)},
                            cfg);
            errorWires.push_back(err);
            cellPorts.at(0).at(wm.first) = voterOutput;
            cellPorts.at(1).at(wm.first) = voterOutput;
            cellPorts.at(2).at(wm.first) = voterOutput;
        }
    }

    for (size_t i = 0; i < tmrx_replication_factor; i++) {
        for (auto wm : wireMap) {
            if (isTmrErrorOutWire(wm.first, cfg)) {
                continue;
            }
            wrapper->connect(wm.second.at(i), cellPorts.at(i).at(wm.first));
        }
    }

    connectErrorSignal(wrapper, errorWires, cfg);

    // Create one uniquified worker clone per TMR path and recursively uniquify
    // all proper submodule cells within each clone so that the three workers
    // are fully independent module hierarchies.
    std::vector<std::string> pathSuffixes = {
        cfg->logicPath1Suffix,
        cfg->logicPath2Suffix,
        cfg->logicPath3Suffix,
    };

    RTLIL::Design *design = mod->design;
    for (size_t i = 0; i < tmrx_replication_factor; i++) {
        RTLIL::IdString workerName = RTLIL::IdString(mod->name.str() + pathSuffixes[i]);

        RTLIL::Module *worker = design->addModule(workerName);
        mod->cloneInto(worker);
        worker->name = workerName;
        worker->set_bool_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE, true);

        uniquifySubmodulesRecursive(worker, pathSuffixes[i], design, cfgMgr);

        duplicates[i]->type = workerName;
    }
    // mod (the template worker) is no longer referenced by any cell.
    // Remove it immediately: the _a/_b/_c workers hold the complete
    // implementation, so the template is redundant. Leaving it in the design
    // as an unreachable orphan causes `stat -top` to crash because stat
    // iterates design->selected_modules() but only builds mod_stat for
    // modules reachable from the top (std::out_of_range on missing keys).
    design->remove(mod);
}
} // namespace TMRX

YOSYS_NAMESPACE_END
