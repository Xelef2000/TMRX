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

        // TODO: remove
        log("args passed: %i\n", args.size());
        for (auto argm : args) {
            log("%s\n", argm);
        }

        std::string config_file = "";

        for (size_t arg = 1; arg < args.size(); arg++) {
            if (args[arg] == "-c" && arg + 1 < args.size()) {
                config_file = args[++arg];
                continue;
            }
            break;
        }

        ConfigManager cfg_mgr(design, config_file);

        log("\n");

        log_header(design, "Sorting modules\n");
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

            log("Processing Module %s\n", mod_name.c_str());
            log_pop();
            log_push();

            const Config *cfg = cfg_mgr.cfg(worker);

            if (cfg->tmr_mode == TmrMode::None) {
                continue;
            }

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
                log("Cloned %s -> %s for TMR expansion\n",
                    worker->name.c_str(), impl_name.c_str());
            }

            if (cfg->tmr_mode == TmrMode::LogicTMR) {
                TMRX::logic_tmr_expansion(target, &cfg_mgr, cfg);
            }

            if (cfg->tmr_mode == TmrMode::FullModuleTMR) {
                TMRX::full_module_tmr_expansion(target, cfg);
            }

            target->fixup_ports();
        }

        log_pop();
    }
} TmrxPass;

PRIVATE_NAMESPACE_END
