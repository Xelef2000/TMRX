#include "tmrx.h"
#include "utils.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

// TODO: Verify if this is the easiest way
struct TMRXMarkPass : public Pass {
    TMRXMarkPass() : Pass("tmrx_mark", "mark ff for later tmrx pass") {}

    void execute(std::vector<std::string>, RTLIL::Design *design) override {
        log_header(design, "Executing Mark Flip Flop Pass\n");
        log_push();

        ff_sources.clear();

        for (auto module : design->modules()) {
            if (design->selected(module) && !module->get_blackbox_attribute()) {
                log("Scanning module %s for FFs\n", log_id(module));
                for (auto cell : module->cells()) {
                    log("Looking at cell %s: ", cell->type.c_str());

                    if (RTLIL::builtin_ff_cell_types().count(cell->type) > 0) {
                        log("Cell is ff");
                        if (cell->has_attribute(ID::src)) {
                            std::string src = cell->get_string_attribute(ID::src);
                            ff_sources[module->name].insert(src);
                        }
                    }
                    log("\n");
                }

                // mark submodules
                for (auto c : module->cells()) {
                    if (design->module(c->type) != nullptr) {
                        design->module(c->type)->set_bool_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE,
                                                                    true);
                    }
                }
            }
        }
        log_pop();
    }
} TMRXMarkPass;

PRIVATE_NAMESPACE_END
