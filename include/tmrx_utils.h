#ifndef TMRX_UTILS_H
#define TMRX_UTILS_H
#include "config_manager.h"
#include "kernel/yosys.h"
#include <vector>

YOSYS_NAMESPACE_BEGIN
namespace TMRX {

bool isProperSubmodule(RTLIL::Module *mod);
bool isFlipFlop(const RTLIL::Cell *cell, const RTLIL::Module *module, const Config *cfg);
bool isClkWire(const RTLIL::Wire *w, const Config *cfg);
bool isClkWire(RTLIL::IdString port, const Config *cfg);
bool isRstWire(RTLIL::IdString port, const Config *cfg);
bool isRstWire(const RTLIL::Wire *w, const Config *cfg);
bool isTmrErrorOutWire(RTLIL::Wire *w, const Config *cfg);

std::pair<std::vector<RTLIL::IdString>, std::vector<RTLIL::IdString>>
getPortNames(const RTLIL::Cell *cell, const RTLIL::Design *design);
RTLIL::IdString createVoterCell(RTLIL::Design *design, size_t wire_width,
                                const std::string &name_prefix);
std::pair<RTLIL::Wire *, RTLIL::Wire *>
insertVoter(RTLIL::Module *module, const std::vector<RTLIL::SigSpec> &inputs, const Config *cfg);

void connectErrorSignal(RTLIL::Module *mod, const std::vector<RTLIL::Wire *> &error_signals,
                        const Config *cfg);

} // namespace TMRX
YOSYS_NAMESPACE_END

#endif
