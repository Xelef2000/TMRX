#ifndef TMRX_LOGIC_EXPANSION_H
#define TMRX_LOGIC_EXPANSION_H

#include "config_manager.h"
#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {

void logic_tmr_expansion(RTLIL::Module *mod, const ConfigManager *cfg_mgr);

}
YOSYS_NAMESPACE_END

#endif
