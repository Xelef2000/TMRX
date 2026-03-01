#include "tmrx_utils.h"
#include "config_manager.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include "tmrx.h"
#include <cstddef>
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
    return ((cfg->clock_port_names.count(w->name) != 0) || w->has_attribute(ID(tmrx_clk_port)));
}

bool is_clk_wire(RTLIL::IdString port, const Config *cfg) {
    return (cfg->clock_port_names.count(port) != 0);
}

bool is_rst_wire(RTLIL::IdString port, const Config *cfg) {
    return (cfg->reset_port_names.count(port) != 0);
}

bool is_rst_wire(const RTLIL::Wire *w, const Config *cfg) {
    return ((cfg->reset_port_names.count(w->name) != 0) || w->has_attribute(ID(tmrx_rst_port)));
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

static std::string get_signal_name(const RTLIL::SigSpec &sig) {
    if (sig.is_wire()) {
        std::string name = sig.as_wire()->name.str();
        if (!name.empty() && name[0] == '\\') {
            name = name.substr(1);
        }
        return name;
    } else if (sig.is_chunk()) {
        const RTLIL::SigChunk &chunk = sig.as_chunk();
        if (chunk.wire != nullptr) {
            std::string name = chunk.wire->name.str();
            if (!name.empty() && name[0] == '\\') {
                name = name.substr(1);
            }
            return name;
        }
    } else {
        for (const auto &chunk : sig.chunks()) {
            if (chunk.wire != nullptr) {
                std::string name = chunk.wire->name.str();
                if (!name.empty() && name[0] == '\\') {
                    name = name.substr(1);
                }
                return name;
            }
        }
    }
    return "const";
}

RTLIL::IdString createVoterCell(RTLIL::Design *design, size_t wire_width, const std::string &name_prefix) {
    RTLIL::IdString voter_name = "\\tmrx_voter_" + name_prefix + "_w" + std::to_string(wire_width);

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

static bool is_port_output(const RTLIL::Cell *cell, RTLIL::IdString port_name, RTLIL::Design *design) {
    RTLIL::Module *cell_mod = design->module(cell->type);
    if (cell_mod) {
        cell_mod->fixup_ports();
        RTLIL::Wire *wire = cell_mod->wire(port_name);
        if (wire) {
            return wire->port_output;
        }
    }
    return cell->output(port_name);
}

static bool is_signal_unconnected(const RTLIL::SigSpec &sig, RTLIL::Module *module) {
    if (sig.is_fully_const()) {
        for (auto &bit : sig.bits()) {
            if (bit.data == RTLIL::State::Sx || bit.data == RTLIL::State::Sz) {
                return true;
            }
        }
        return false;
    }

    RTLIL::Design *design = module->design;

    pool<RTLIL::SigBit> driven_bits;

    for (auto wire : module->wires()) {
        if (wire->port_input) {
            for (int i = 0; i < wire->width; i++) {
                driven_bits.insert(RTLIL::SigBit(wire, i));
            }
        }
    }

    for (auto cell : module->cells()) {
        for (auto &conn : cell->connections()) {
            if (is_port_output(cell, conn.first, design)) {
                for (auto &bit : conn.second.bits()) {
                    if (bit.wire != nullptr) {
                        driven_bits.insert(bit);
                    }
                }
            }
        }
    }

    for (auto &conn : module->connections()) {
        for (auto &bit : conn.first.bits()) {
            if (bit.wire != nullptr) {
                driven_bits.insert(bit);
            }
        }
    }

    for (auto &bit : sig.bits()) {
        if (bit.wire != nullptr && driven_bits.count(bit) == 0) {
            return true;
        }
    }

    return false;
}

static bool is_signal_constant(const RTLIL::SigSpec &sig) {
    if (!sig.is_fully_const()) {
        return false;
    }
    for (auto &bit : sig.bits()) {
        if (bit.data != RTLIL::State::S0 && bit.data != RTLIL::State::S1) {
            return false;
        }
    }
    return true;
}

std::pair<RTLIL::Wire *, RTLIL::Wire *>
insert_voter(RTLIL::Module *module, const std::vector<RTLIL::SigSpec> &inputs, const Config *cfg) {
    if (inputs.size() != 3) {
        log_error("Voters are only intended to be inserted with 3 inputs");
    }

    RTLIL::Design *design = module->design;

    size_t wire_width = inputs.at(0).size();

    std::string sig_name_a = get_signal_name(inputs.at(0));
    std::string sig_name_b = get_signal_name(inputs.at(1));
    std::string sig_name_c = get_signal_name(inputs.at(2));

    std::string mod_name = module->name.str();
    if (!mod_name.empty() && mod_name[0] == '\\') {
        mod_name = mod_name.substr(1);
    }

    std::string name_prefix = mod_name + "_" + sig_name_a + "_" + sig_name_b + "_" + sig_name_c;

    if (cfg->tmr_voter_safe_mode) {
        std::vector<std::string> input_names = {"a", "b", "c"};

        for (size_t i = 0; i < 3; i++) {
            const RTLIL::SigSpec &sig = inputs.at(i);

            if (is_signal_unconnected(sig, module)) {
                log_error("TMRX Safe Mode: Voter input '%s' in module '%s' is not connected (signal: %s). "
                          "All voter inputs must be driven.\n",
                          input_names[i].c_str(), module->name.c_str(), log_signal(sig));
            }

            if (is_signal_constant(sig)) {
                log_warning("TMRX Safe Mode: Voter input '%s' in module '%s' is connected to a constant value (signal: %s). "
                            "This may indicate a configuration or connection issue.\n",
                            input_names[i].c_str(), module->name.c_str(), log_signal(sig));
            }
        }
    }

    RTLIL::IdString voter_name = createVoterCell(design, wire_width, name_prefix);

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

void connect_error_signal(RTLIL::Module *mod,const std::vector<RTLIL::Wire *> &error_signals) {
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
}
YOSYS_NAMESPACE_END
