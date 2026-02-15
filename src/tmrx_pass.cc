#include "config_manager.h"
#include "kernel/hashlib.h"
#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "tmrx.h"
#include "utils.h"
#include <cstddef>

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
            if (args[arg] == "-config" && arg+1 < args.size()) {
                config_file = args[++arg];
                continue;
            }
            break;
        }


        ConfigManager cfg_mgr(design, config_file);


        // for()

        // ff_sources[module->name].insert("absval_source");
        //
        log_pop();
    }
} TmrxPass;

PRIVATE_NAMESPACE_END
