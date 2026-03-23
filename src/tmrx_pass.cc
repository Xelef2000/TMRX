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

        std::string config_file = "";

        for (size_t arg = 1; arg < args.size(); arg++) {
            if (args[arg] == "-c" && arg + 1 < args.size()) {
                config_file = args[++arg];
                continue;
            }
            break;
        }

        ConfigManager cfg_mgr(design, config_file);

        TopoSort<RTLIL::IdString> modules_to_process;

        for (auto module : design->modules()) {
            if (!design->selected(module) || module->get_blackbox_attribute()) {
                continue;
            }

            modules_to_process.node(module->name);

            for (auto c : module->cells()) {
                if (design->module(c->type) &&
                    (TMRX::is_proper_submodule(design->module(c->type)))) {
                    modules_to_process.edge(module->name, c->type);
                }
            }
        }

        modules_to_process.sort();

        for (auto it = modules_to_process.sorted.rbegin(); it != modules_to_process.sorted.rend();
             ++it) {
            RTLIL::IdString mod_name = *it;
            RTLIL::Module *worker = design->module(mod_name);
            if (!worker)
                continue;

            // Blackbox modules (standard cells, IP blackboxes) that appear as
            // edge-only nodes in the topo sort must not be expanded. They were
            // intentionally excluded from the node-building loop above.
            if (worker->get_blackbox_attribute())
                continue;

            const Config *cfg = cfg_mgr.cfg(worker);

            if (cfg->tmr_mode == TmrMode::None) {
                continue;
            }

            log("Processing module '%s' [%s]\n", mod_name.c_str(),
                tmr_mode_to_string(cfg->tmr_mode).c_str());

            // When preserve_module_ports=false and this is a proper submodule,
            // clone the module before expansion. The clone is what gets expanded
            // (triplicated ports etc.) while the original keeps its interface
            // unchanged so that FullModuleTMR parents can connect to it without
            // port mismatches. LogicTMR parents remap cell types to the clone
            // via the tmrx_impl_module attribute.
            RTLIL::Module *target = worker;
            if (!cfg->preserve_module_ports && TMRX::is_proper_submodule(worker)) {
                RTLIL::IdString impl_name =
                    RTLIL::IdString(worker->name.str() + "_tmrx_impl");
                target = design->addModule(impl_name);
                worker->cloneInto(target);
                target->name = impl_name;
                target->set_bool_attribute(ID(tmrx_is_proper_submodule), true);
                worker->set_string_attribute(ID(tmrx_impl_module), impl_name.str());
                log("  Cloned '%s' -> '%s' for port-preserving expansion\n",
                    worker->name.c_str(), impl_name.c_str());
            }

            if (cfg->tmr_mode == TmrMode::LogicTMR) {
                TMRX::logic_tmr_expansion(target, &cfg_mgr, cfg);
                target->fixup_ports();
            }

            if (cfg->tmr_mode == TmrMode::FullModuleTMR) {
                // full_module_tmr_expansion removes `target` (the _tmrx_worker
                // template) from the design at the end, so `target` is a
                // dangling pointer after the call. The wrapper module created
                // inside already calls fixup_ports() before that point.
                TMRX::full_module_tmr_expansion(target, cfg);
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
        std::vector<RTLIL::IdString> to_remove;
        for (auto module : design->modules()) {
            if (module->has_attribute(ID(tmrx_impl_module))) {
                to_remove.push_back(module->name);
            }
        }
        for (auto name : to_remove) {
            bool still_referenced = false;
            for (auto mod : design->modules()) {
                for (auto cell : mod->cells()) {
                    if (cell->type == name) {
                        still_referenced = true;
                        break;
                    }
                }
                if (still_referenced)
                    break;
            }
            if (still_referenced) {
                log("Keeping original module '%s' (still referenced by None-mode parent)\n",
                    name.c_str());
            } else {
                log("Removing original module '%s' (replaced by _tmrx_impl clone)\n",
                    name.c_str());
                design->remove(design->module(name));
            }
        }

        log_pop();
    }
} TmrxPass;

PRIVATE_NAMESPACE_END
