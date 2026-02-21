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

            if (cfg_mgr.cfg(worker)->tmr_mode == Config::TmrMode::None) {
                continue;
            }

            if (cfg_mgr.cfg(worker)->tmr_mode == Config::TmrMode::LogicTMR) {
                TMRX::logic_tmr_expansion(worker, &cfg_mgr);
            }

            if (cfg_mgr.cfg(worker)->tmr_mode == Config::TmrMode::FullModuleTMR) {
                TMRX::full_module_tmr_expansion(worker, cfg_mgr.cfg(worker));
            }

            worker->fixup_ports();
        }

        log_pop();
    }
} TmrxPass;

PRIVATE_NAMESPACE_END
