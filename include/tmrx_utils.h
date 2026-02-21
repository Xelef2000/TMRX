#ifndef TMRX_UTILS_H
#define TMRX_UTILS_H
#include "config_manager.h"
#include "kernel/yosys.h"
#include <vector>

YOSYS_NAMESPACE_BEGIN
namespace TMRX {

bool is_proper_submodule(RTLIL::Module *mod);
bool is_flip_flop(const RTLIL::Cell *cell, const RTLIL::Module *module, const Config *cfg);
bool is_clk_wire(const RTLIL::Wire *w, const Config *cfg);
bool is_clk_wire(RTLIL::IdString port, const Config *cfg);
bool is_rst_wire(RTLIL::IdString port, const Config *cfg);
bool is_rst_wire(const RTLIL::Wire *w, const Config *cfg);
bool is_tmr_error_out_wire(RTLIL::Wire *w);

std::pair<std::vector<RTLIL::IdString>, std::vector<RTLIL::IdString>>
get_port_names(const RTLIL::Cell *cell, const RTLIL::Design *design);
RTLIL::IdString createVoterCell(RTLIL::Design *design, size_t wire_width);
std::pair<RTLIL::Wire *, RTLIL::Wire *>
insert_voter(RTLIL::Module *module, std::vector<RTLIL::SigSpec> inputs, RTLIL::Design *design);

void connect_error_signal(RTLIL::Module *mod, std::vector<RTLIL::Wire *> error_signals);

} // namespace TMRX
YOSYS_NAMESPACE_END

#endif
