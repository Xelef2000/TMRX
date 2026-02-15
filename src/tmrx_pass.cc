#include "tmrx.h"
#include "utils.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct TmrxPass : public Pass {
    TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}

    void execute(std::vector<std::string>, RTLIL::Design *design) override
    {
        log_header(design, "Executing TMRX pass (Triple Modular Redundancy).\n");
        log_push();
        // ff_sources[module->name].insert("absval_source");
    }
} TmrxPass;

PRIVATE_NAMESPACE_END
