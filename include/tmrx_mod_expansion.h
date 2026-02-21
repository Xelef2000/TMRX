#ifndef TMRX_MOD_EXPANSION_H
#define TMRX_MOD_EXPANSION_H

#include "config_manager.h"
#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {

void full_module_tmr_expansion(RTLIL::Module *mod, const Config *cfg);

}
YOSYS_NAMESPACE_END

#endif
