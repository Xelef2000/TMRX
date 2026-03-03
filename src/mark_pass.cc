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
                int ff_count = 0;
                int submod_count = 0;

                for (auto cell : module->cells()) {
                    if (RTLIL::builtin_ff_cell_types().count(cell->type) > 0) {
                        ff_count++;
                        if (cell->has_attribute(ID::src)) {
                            std::string src = cell->get_string_attribute(ID::src);
                            ff_sources[module->name].insert(src);
                        }
                    }
                }

                // mark submodules
                for (auto c : module->cells()) {
                    if (design->module(c->type) != nullptr) {
                        design->module(c->type)->set_bool_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE,
                                                                    true);
                        submod_count++;
                    }
                }

                log("Module '%s': %d flip-flop(s), %d submodule instance(s)\n",
                    log_id(module), ff_count, submod_count);
            }
        }
        log_pop();
    }
} TMRXMarkPass;

PRIVATE_NAMESPACE_END
