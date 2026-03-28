#include "config_manager.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include "tmrx.h"
#include "tmrx_logic_expansion.h"
#include "tmrx_mod_expansion.h"
#include "tmrx_utils.h"
#include "utils.h"
#include <cmath>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct TmrxPass : public Pass {
  private:
  public:
    TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}

    void execute(std::vector<std::string> args, RTLIL::Design *design) override {
        log_header(design, "Executing TMRX pass (Triple Modular Redundancy).\n");
        log_push();

        std::string configFile = "";

        for (size_t arg = 1; arg < args.size(); arg++) {
            if (args[arg] == "-c" && arg + 1 < args.size()) {
                configFile = args[++arg];
                continue;
            }
            break;
        }

        TMRX::ConfigManager cfgMgr(design, configFile);

        TopoSort<RTLIL::IdString> modulesToProcess;

        for (auto module : design->modules()) {
            if (!design->selected(module) || module->get_blackbox_attribute()) {
                continue;
            }

            modulesToProcess.node(module->name);

            for (auto c : module->cells()) {
                if (design->module(c->type) && (TMRX::isProperSubmodule(design->module(c->type)))) {
                    modulesToProcess.edge(module->name, c->type);
                }
            }
        }

        modulesToProcess.sort();

        for (auto it = modulesToProcess.sorted.rbegin(); it != modulesToProcess.sorted.rend();
             ++it) {
            RTLIL::IdString moduleName = *it;
            RTLIL::Module *worker = design->module(moduleName);
            if (!worker)
                continue;

            // Blackbox modules (standard cells, IP blackboxes) that appear as
            // edge-only nodes in the topo sort must not be expanded. They were
            // intentionally excluded from the node-building loop above.
            if (worker->get_blackbox_attribute())
                continue;

            const TMRX::Config *cfg = cfgMgr.getConfig(worker);

            if (cfg->tmrMode == TMRX::TmrMode::None) {
                continue;
            }

            log("Processing module '%s' [%s]\n", moduleName.c_str(),
                TMRX::tmrModeToString(cfg->tmrMode).c_str());

            // When preserve_module_ports=false and this is a proper submodule,
            // clone the module before expansion. The clone is what gets expanded
            // (triplicated ports etc.) while the original keeps its interface
            // unchanged so that FullModuleTMR parents can connect to it without
            // port mismatches. LogicTMR parents remap cell types to the clone
            // via the tmrx_impl_module attribute.
            RTLIL::Module *target = worker;
            if (!cfg->preserveModulePorts && TMRX::isProperSubmodule(worker)) {
                RTLIL::IdString implName =
                    RTLIL::IdString(worker->name.str() + TMRX::tmrx_impl_module_suffix);
                target = design->addModule(implName);
                worker->cloneInto(target);
                target->name = implName;
                target->set_bool_attribute(TMRX::ATTRIBUTE_IS_PROPER_SUBMODULE, true);
                worker->set_string_attribute(TMRX::ATTRIBUTE_IMPL_MODULE, implName.str());
                log("  Cloned '%s' -> '%s' for port-preserving expansion\n", worker->name.c_str(),
                    implName.c_str());
            }

            if (cfg->tmrMode == TMRX::TmrMode::LogicTMR) {
                TMRX::logicTmrExpansion(target, &cfgMgr, cfg);
                target->fixup_ports();
            }

            if (cfg->tmrMode == TMRX::TmrMode::FullModuleTMR) {
                // fullModuleTmrExpansion removes `target` (the _tmrx_worker
                // template) from the design at the end, so `target` is a
                // dangling pointer after the call. The wrapper module created
                // inside already calls fixup_ports() before that point.
                TMRX::fullModuleTmrExpansion(target, cfg);
            }
        }

        // Remove original modules that were cloned to _tmrx_impl, but only if
        // no module in the design still references them. A None-mode parent is
        // never remapped to the _tmrx_impl variant, so it may still hold a
        // cell whose type points to the original. In that case the original
        // must be kept; it is reachable from the top through the None-mode
        // parent, so `stat -top` will not crash. Originals with no remaining
        // references are safe to delete (keeping them would cause the stat
        // crash described above, because stat only builds mod_stat for modules
        // reachable from the top).
        std::vector<RTLIL::IdString> toRemove;
        for (auto module : design->modules()) {
            if (module->has_attribute(TMRX::ATTRIBUTE_IMPL_MODULE)) {
                toRemove.push_back(module->name);
            }
        }
        for (auto name : toRemove) {
            bool stillReferenced = false;
            for (auto mod : design->modules()) {
                for (auto cell : mod->cells()) {
                    if (cell->type == name) {
                        stillReferenced = true;
                        break;
                    }
                }
                if (stillReferenced)
                    break;
            }
            if (stillReferenced) {
                log("Keeping original module '%s' (still referenced by None-mode parent)\n",
                    name.c_str());
            } else {
                log("Removing original module '%s' (replaced by _tmrx_impl clone)\n", name.c_str());
                design->remove(design->module(name));
            }
        }

        log_pop();
    }
} TmrxPass;

PRIVATE_NAMESPACE_END
