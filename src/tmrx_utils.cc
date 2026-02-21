#include "tmrx_utils.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "tmrx.h"
YOSYS_NAMESPACE_BEGIN

namespace TMRX {

bool is_proper_submodule(RTLIL::Module *mod) {
    // TODO: find alternative
    if ((mod != nullptr) && mod->has_attribute(ID(tmrx_is_proper_submodule))) {
        return mod->get_bool_attribute(ID(tmrx_is_proper_submodule));
    }

    return false;
}

bool is_flip_flop(const RTLIL::Cell *cell, const RTLIL::Module *module, const Config *cfg) {

    if (cfg->excluded_ff_cells.count(cell->type) != 0) {
        return false;
    }

    if (RTLIL::builtin_ff_cell_types().count(cell->type) > 0) {
        return true;
    }

    std::string src = cell->get_string_attribute(ID::src);
    if (ff_sources[module->name].count(src) > 0) {
        return true;
    }

    if (cfg->ff_cells.count(cell->type) > 0 || cfg->additional_ff_cells.count(cell->type) > 0) {
        return true;
    }

    return false;
}

// Move ids to header
//  TODO: check attr if it is a submodule port
bool is_clk_wire(const RTLIL::Wire *w, const Config *cfg) {
    return ((w->name == cfg->clock_port_name) || w->has_attribute(ID(tmrx_clk_port)));
}

bool is_clk_wire(RTLIL::IdString port, const Config *cfg) {
    return ((port == cfg->clock_port_name));
}

bool is_rst_wire(RTLIL::IdString port, const Config *cfg) {
    return ((port == cfg->reset_port_name));
}

bool is_rst_wire(const RTLIL::Wire *w, const Config *cfg) {
    return ((w->name == cfg->reset_port_name) || w->has_attribute(ID(tmrx_rst_port)));
}

bool is_tmr_error_out_wire(RTLIL::Wire *w) { return (w->has_attribute(ID(tmrx_error_sink))); }

std::pair<std::vector<RTLIL::IdString>, std::vector<RTLIL::IdString>>
get_port_names(const RTLIL::Cell *cell, const RTLIL::Design *design) {
    std::vector<RTLIL::IdString> outputs = {};
    std::vector<RTLIL::IdString> inputs = {};

    const RTLIL::Module *cell_mod = design->module(cell->type);
    if (cell_mod) {
        const_cast<RTLIL::Module *>(cell_mod)->fixup_ports();
        for (auto &conn : cell->connections()) {
            const RTLIL::Wire *wire = cell_mod->wire(conn.first);
            if (wire && wire->port_output) {
                outputs.push_back(conn.first);
            }

            if (wire && wire->port_input) {
                inputs.push_back(conn.first);
            }
        }
    } else {
        for (auto &conn : cell->connections()) {
            if (cell->output(conn.first)) {
                outputs.push_back(conn.first);
            }
            if (cell->input(conn.first)) {
                inputs.push_back(conn.first);
            }
        }
    }

    return {inputs, outputs};
}

RTLIL::IdString createVoterCell(RTLIL::Design *design, size_t wire_width) {
    // TODO: make voter behaviour more predictable
    RTLIL::IdString voter_name = "\\tmrx_simple_voter_" + std::to_string(wire_width);

    if (design->module(voter_name) != nullptr) {
        return voter_name;
    }

    RTLIL::Module *voter = design->addModule(voter_name);

    voter->attributes[ID::keep_hierarchy] = RTLIL::State::S1;

    RTLIL::Wire *in_a = voter->addWire("\\a", wire_width);
    RTLIL::Wire *in_b = voter->addWire("\\b", wire_width);
    RTLIL::Wire *in_c = voter->addWire("\\c", wire_width);
    in_c->port_input = true;
    in_a->port_input = true;
    in_b->port_input = true;

    RTLIL::Wire *out_y = voter->addWire("\\y", wire_width);
    out_y->port_output = true;
    RTLIL::Wire *out_err = voter->addWire("\\err", wire_width);
    out_err->port_output = true;

    RTLIL::SigSpec pair1 = voter->And(NEW_ID, in_a, in_b);
    RTLIL::SigSpec pair2 = voter->And(NEW_ID, in_a, in_c);
    RTLIL::SigSpec pair3 = voter->And(NEW_ID, in_b, in_c);

    RTLIL::SigSpec intermediate1 = voter->Or(NEW_ID, pair1, pair2);
    voter->addOr(NEW_ID, intermediate1, pair3, out_y);

    RTLIL::SigSpec err_pair1 = voter->Xor(NEW_ID, in_a, in_b);
    RTLIL::SigSpec err_pair2 = voter->Xor(NEW_ID, in_b, in_c);
    voter->addOr(NEW_ID, err_pair1, err_pair2, out_err);

    voter->fixup_ports();
    return voter_name;
}

// TODO: remove mod and design
std::pair<RTLIL::Wire *, RTLIL::Wire *>
insert_voter(RTLIL::Module *module, std::vector<RTLIL::SigSpec> inputs, RTLIL::Design *design) {
    if (inputs.size() != 3) {
        log_error("Voters are only intended to be inserted with 3 inputs");
    }

    size_t wire_width = inputs.at(0).size();

    RTLIL::IdString voter_name = createVoterCell(design, wire_width);

    RTLIL::Wire *last_wire = module->addWire(NEW_ID, wire_width);
    RTLIL::Wire *err_wire = module->addWire(NEW_ID, wire_width);
    RTLIL::Cell *voter_inst = module->addCell(NEW_ID, voter_name);

    voter_inst->setPort("\\a", inputs.at(0));
    voter_inst->setPort("\\b", inputs.at(1));
    voter_inst->setPort("\\c", inputs.at(2));
    voter_inst->setPort("\\y", last_wire);
    voter_inst->setPort("\\err", err_wire);

    return {last_wire, err_wire};
}

void connect_error_signal(RTLIL::Module *mod, std::vector<RTLIL::Wire *> error_signals) {
    log_header(mod->design, "Connecting Error Signals\n");

    RTLIL::Wire *sink = nullptr;
    for (auto w : mod->wires()) {
        if (!w->port_output) {
            continue;
        }
        if (is_tmr_error_out_wire(w)) {
            if (sink != nullptr) {
                log_error("Duplicate error sinks, only one allowed");
            }
            sink = w;
        }
    }
    if (sink != nullptr && !error_signals.empty()) {
        RTLIL::IdString sink_name = sink->name;
        mod->rename(sink, mod->uniquify(sink_name.str() + "_old"));
        sink->port_output = false;
        RTLIL::Wire *new_error = mod->addWire(sink_name, sink->width);

        new_error->port_output = true;
        sink->port_output = false;
        new_error->upto = sink->upto;
        new_error->attributes = sink->attributes;
        mod->fixup_ports();

        RTLIL::SigSpec last_wire = sink;
        for (auto s : error_signals) {
            last_wire = mod->Or(NEW_ID, last_wire, s);
        }
        mod->connect(new_error, last_wire);
    }
}
} // namespace TMRX
YOSYS_NAMESPACE_END
