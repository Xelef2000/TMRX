#include "tmrx_utils.h"
#include "config_manager.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include "tmrx.h"
#include <cstddef>
#include <cstdint>
YOSYS_NAMESPACE_BEGIN

namespace TMRX {

bool isProperSubmodule(RTLIL::Module *mod) {
    // TODO: find alternative
    if ((mod != nullptr) && mod->has_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE)) {
        return mod->get_bool_attribute(ATTRIBUTE_IS_PROPER_SUBMODULE);
    }

    return false;
}

bool isFlipFlop(const RTLIL::Cell *cell, const RTLIL::Module *module, const Config *cfg) {

    if (cfg->excludedFfCells.count(cell->type) != 0) {
        return false;
    }

    if (RTLIL::builtin_ff_cell_types().count(cell->type) > 0) {
        return true;
    }

    std::string src = cell->get_string_attribute(ID::src);
    if (ffSources[module->name].count(src) > 0) {
        return true;
    }

    if (cfg->ffCells.count(cell->type) > 0 || cfg->additionalFfCells.count(cell->type) > 0) {
        return true;
    }

    return false;
}

// Move ids to header
//  TODO: check attr if it is a submodule port
bool isClkWire(const RTLIL::Wire *w, const Config *cfg) {
    return ((cfg->clockPortNames.count(w->name) != 0) || w->has_attribute(ATTRIBUTE_CLK_PORT));
}

bool isClkWire(RTLIL::IdString port, const Config *cfg) {
    return (cfg->clockPortNames.count(port) != 0);
}

bool isRstWire(RTLIL::IdString port, const Config *cfg) {
    return (cfg->resetPortNames.count(port) != 0);
}

bool isRstWire(const RTLIL::Wire *w, const Config *cfg) {
    return ((cfg->resetPortNames.count(w->name) != 0) || w->has_attribute(ATTRIBUTE_RST_PORT));
}

bool isTmrErrorOutWire(RTLIL::Wire *w, const Config *cfg) {
    if (w->has_attribute(ATTRIBUTE_ERROR_SINK))
        return true;
    if (cfg && !cfg->errorPortName.empty())
        return w->name == makeRtlilId(cfg->errorPortName);
    return false;
}

std::pair<std::vector<RTLIL::IdString>, std::vector<RTLIL::IdString>>
getPortNames(const RTLIL::Cell *cell, const RTLIL::Design *design) {
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

static std::string getSignalName(const RTLIL::SigSpec &sig) {
    for (const auto &chunk : sig.chunks()) {
        if (chunk.wire != nullptr) {
            std::string name = chunk.wire->name.str();
            if (!name.empty() && name[0] == '\\')
                name = name.substr(1);
            return name;
        }
    }
    return tmrx_signal_name_const;
}

static RTLIL::IdString getDomainAttributeName(const std::string &suffix) {
    return RTLIL::IdString("\\tmr_domain" + suffix);
}

static void setCellDomainAttribute(RTLIL::Cell *cell, const std::string &suffix) {
    if (!suffix.empty()) {
        cell->set_bool_attribute(getDomainAttributeName(suffix), true);
    }
}

static std::string sanitizeIdentifierComponent(const std::string &value) {
    std::string result;
    result.reserve(value.size());

    bool lastWasUnderscore = false;
    for (char ch : value) {
        bool isAlphaNum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                          (ch >= '0' && ch <= '9');
        char out = (isAlphaNum || ch == '_') ? ch : '_';

        if (out == '_' && lastWasUnderscore) {
            continue;
        }

        result.push_back(out);
        lastWasUnderscore = (out == '_');
    }

    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }

    if (result.empty()) {
        return "anon";
    }

    return result;
}

static uint64_t hashStringFnv1a(const std::string &value) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

static std::string shortenIdentifierComponent(const std::string &value, size_t maxLength) {
    if (value.size() <= maxLength) {
        return value;
    }

    const std::string hashSuffix = stringf("_%016llx",
                                           static_cast<unsigned long long>(hashStringFnv1a(value)));
    if (maxLength <= hashSuffix.size()) {
        return hashSuffix.substr(hashSuffix.size() - maxLength);
    }

    return value.substr(0, maxLength - hashSuffix.size()) + hashSuffix;
}

static std::string appendDomainTag(const std::string &name_prefix, const std::string &domainSuffix) {
    if (domainSuffix.empty()) {
        return name_prefix;
    }

    return name_prefix + tmrx_signal_name_separator + "tmr_domain" + domainSuffix;
}

RTLIL::IdString createVoterCell(RTLIL::Design *design, size_t wire_width,
                                const std::string &name_prefix) {
    RTLIL::IdString voter_name = std::string(tmrx_voter_module_prefix) + name_prefix +
                                 tmrx_voter_width_separator + std::to_string(wire_width);

    if (design->module(voter_name) != nullptr) {
        return voter_name;
    }

    RTLIL::Module *voter = design->addModule(voter_name);
    voter->attributes[ID::keep_hierarchy] = RTLIL::State::S1;

    RTLIL::Wire *in_a = voter->addWire(tmrx_voter_port_a_id, wire_width);
    RTLIL::Wire *in_b = voter->addWire(tmrx_voter_port_b_id, wire_width);
    RTLIL::Wire *in_c = voter->addWire(tmrx_voter_port_c_id, wire_width);
    in_c->port_input = true;
    in_a->port_input = true;
    in_b->port_input = true;

    RTLIL::Wire *out_y = voter->addWire(tmrx_voter_port_y_id, wire_width);
    out_y->port_output = true;
    RTLIL::Wire *out_err = voter->addWire(tmrx_voter_port_err_id, wire_width);
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

// Clone the user-supplied custom voter template into a uniquely named module
// that follows the same \tmrx_voter_<prefix>_w1 convention as the built-in
// voter.  Each unique name_prefix gets its own clone; subsequent calls with
// the same prefix simply return the already-created module name.
static RTLIL::IdString createCustomVoterCell(RTLIL::Design *design,
                                             const std::string &template_module,
                                             const std::string &name_prefix) {
    RTLIL::IdString unique_name = std::string(tmrx_voter_module_prefix) + name_prefix +
                                  tmrx_voter_width_separator + std::to_string(1);

    if (design->module(unique_name) != nullptr)
        return unique_name;

    RTLIL::IdString template_id = makeRtlilId(template_module);
    RTLIL::Module *tmpl = design->module(template_id);
    if (tmpl == nullptr)
        log_error("Custom voter template module '%s' not found in design.\n",
                  template_module.c_str());

    RTLIL::Module *clone = design->addModule(unique_name);
    tmpl->cloneInto(clone);
    clone->name = unique_name;
    clone->attributes[ID::keep_hierarchy] = RTLIL::State::S1;
    return unique_name;
}

static bool isPortOutput(const RTLIL::Cell *cell, RTLIL::IdString port_name,
                         RTLIL::Design *design) {
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

static bool isSignalUnconnected(const RTLIL::SigSpec &sig, RTLIL::Module *module) {
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
            if (isPortOutput(cell, conn.first, design)) {
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

static bool isSignalConstant(const RTLIL::SigSpec &sig) {
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
insertVoter(RTLIL::Module *module, const std::vector<RTLIL::SigSpec> &inputs, const Config *cfg,
            const std::string &domainSuffix) {
    if (inputs.size() != tmrx_replication_factor) {
        log_error("Voters are only intended to be inserted with %zu inputs\n",
                  tmrx_replication_factor);
    }

    RTLIL::Design *design = module->design;
    size_t wire_width = inputs.at(0).size();

    std::string sig_name_a = getSignalName(inputs.at(0));
    std::string sig_name_b = getSignalName(inputs.at(1));
    std::string sig_name_c = getSignalName(inputs.at(2));

    std::string modName = module->name.str();
    if (!modName.empty() && modName[0] == '\\')
        modName = modName.substr(1);

    std::string name_prefix = modName + tmrx_signal_name_separator + sig_name_a +
                              tmrx_signal_name_separator + sig_name_b + tmrx_signal_name_separator +
                              sig_name_c;
    std::string voter_name_prefix =
        shortenIdentifierComponent(sanitizeIdentifierComponent(appendDomainTag(name_prefix, domainSuffix)),
                                   72);

    if (cfg->tmrVoterSafeMode) {
        for (size_t i = 0; i < tmrx_replication_factor; i++) {
            const RTLIL::SigSpec &sig = inputs.at(i);
            if (isSignalUnconnected(sig, module)) {
                log_error("TMRX Safe Mode: Voter input '%s' in module '%s' is not connected "
                          "(signal: %s). "
                          "All voter inputs must be driven.\n",
                          tmrx_voter_input_labels[i], module->name.c_str(), log_signal(sig));
            }
            if (isSignalConstant(sig)) {
                log_warning("TMRX Safe Mode: Voter input '%s' in module '%s' is connected to a "
                            "constant value (signal: %s). "
                            "This may indicate a configuration or connection issue.\n",
                            tmrx_voter_input_labels[i], module->name.c_str(), log_signal(sig));
            }
        }
    } else {
        // Optimization: if all 3 inputs are electrically identical, skip voter insertion.
        SigMap sigmap(module);
        if (sigmap(inputs.at(0)) == sigmap(inputs.at(1)) &&
            sigmap(inputs.at(1)) == sigmap(inputs.at(2))) {
            RTLIL::Wire *last_wire = module->addWire(NEW_ID, wire_width);
            RTLIL::Wire *err_wire = module->addWire(NEW_ID, 1);
            module->connect(last_wire, inputs.at(0));
            module->connect(err_wire, RTLIL::SigSpec(RTLIL::State::S0, 1));
            return {last_wire, err_wire};
        }
    }

    // Resolve the 1-bit voter cell: both paths produce a uniquely named module
    // following the \tmrx_voter_<prefix>_w1 convention.
    RTLIL::IdString voter_1bit =
        (cfg->tmrVoter == TmrVoter::Custom)
            ? createCustomVoterCell(design, cfg->tmrVoterModule, voter_name_prefix)
            : createVoterCell(design, 1, voter_name_prefix);

    RTLIL::SigSpec output_bits;
    RTLIL::SigSpec error_bits;

    // Resolve the voter's clock and reset port names.
    // Config option takes priority over the tmrx_clk_port / tmrx_rst_port Verilog attribute.
    RTLIL::Module *voter_mod_ptr = design->module(voter_1bit);
    RTLIL::IdString voter_clk_port;
    RTLIL::IdString voter_rst_port;

    if (voter_mod_ptr) {
        if (!cfg->tmrVoterClockPortName.empty()) {
            voter_clk_port = makeRtlilId(cfg->tmrVoterClockPortName);
        } else {
            for (auto port_id : voter_mod_ptr->ports) {
                RTLIL::Wire *pw = voter_mod_ptr->wire(port_id);
                if (pw && pw->port_input && pw->has_attribute(ATTRIBUTE_CLK_PORT)) {
                    voter_clk_port = port_id;
                    break;
                }
            }
        }

        if (!cfg->tmrVoterResetPortName.empty()) {
            voter_rst_port = makeRtlilId(cfg->tmrVoterResetPortName);
        } else {
            for (auto port_id : voter_mod_ptr->ports) {
                RTLIL::Wire *pw = voter_mod_ptr->wire(port_id);
                if (pw && pw->port_input && pw->has_attribute(ATTRIBUTE_RST_PORT)) {
                    voter_rst_port = port_id;
                    break;
                }
            }
        }
    }

    // Find the parent wire to drive the voter's clock / reset port.
    // If tmr_voter_clock_net / tmr_voter_reset_net is set, use that specific wire;
    // otherwise fall back to the first input wire recognised as a clock / reset port.
    RTLIL::Wire *parent_clk_wire = nullptr;
    RTLIL::Wire *parent_rst_wire = nullptr;

    if (!voter_clk_port.empty()) {
        if (!cfg->tmrVoterClockNet.empty()) {
            parent_clk_wire = module->wire(makeRtlilId(cfg->tmrVoterClockNet));
            if (!parent_clk_wire)
                log_warning("Custom voter '%s': tmr_voter_clock_net '%s' not found in "
                            "parent module '%s'.\n",
                            voter_1bit.c_str(), cfg->tmrVoterClockNet.c_str(),
                            module->name.c_str());
        } else {
            for (auto wire : module->wires()) {
                if (wire->port_input && isClkWire(wire, cfg)) {
                    parent_clk_wire = wire;
                    break;
                }
            }
            if (!parent_clk_wire)
                log_warning("Custom voter '%s': clock port '%s' specified but no clock "
                            "wire found in parent module '%s'.\n",
                            voter_1bit.c_str(), voter_clk_port.c_str(), module->name.c_str());
        }
    }

    if (!voter_rst_port.empty()) {
        if (!cfg->tmrVoterResetNet.empty()) {
            parent_rst_wire = module->wire(makeRtlilId(cfg->tmrVoterResetNet));
            if (!parent_rst_wire)
                log_warning("Custom voter '%s': tmr_voter_reset_net '%s' not found in "
                            "parent module '%s'.\n",
                            voter_1bit.c_str(), cfg->tmrVoterResetNet.c_str(),
                            module->name.c_str());
        } else {
            for (auto wire : module->wires()) {
                if (wire->port_input && isRstWire(wire, cfg)) {
                    parent_rst_wire = wire;
                    break;
                }
            }
            if (!parent_rst_wire)
                log_warning("Custom voter '%s': reset port '%s' specified but no reset "
                            "wire found in parent module '%s'.\n",
                            voter_1bit.c_str(), voter_rst_port.c_str(), module->name.c_str());
        }
    }

    for (size_t bit = 0; bit < wire_width; bit++) {
        RTLIL::Wire *bit_out = module->addWire(NEW_ID, 1);
        RTLIL::Wire *bit_err = module->addWire(NEW_ID, 1);

        RTLIL::Cell *voter_inst = module->addCell(NEW_ID, voter_1bit);
        setCellDomainAttribute(voter_inst, domainSuffix);
        voter_inst->setPort(tmrx_voter_port_a_id, inputs.at(0).extract(bit, 1));
        voter_inst->setPort(tmrx_voter_port_b_id, inputs.at(1).extract(bit, 1));
        voter_inst->setPort(tmrx_voter_port_c_id, inputs.at(2).extract(bit, 1));
        voter_inst->setPort(tmrx_voter_port_y_id, bit_out);
        voter_inst->setPort(tmrx_voter_port_err_id, bit_err);

        if (!voter_clk_port.empty() && parent_clk_wire)
            voter_inst->setPort(voter_clk_port, parent_clk_wire);
        if (!voter_rst_port.empty() && parent_rst_wire)
            voter_inst->setPort(voter_rst_port, parent_rst_wire);

        output_bits.append(bit_out);
        error_bits.append(bit_err);
    }

    if (!domainSuffix.empty() && voter_mod_ptr != nullptr) {
        for (auto voter_cell : voter_mod_ptr->cells()) {
            setCellDomainAttribute(voter_cell, domainSuffix);
        }
    }

    // Reassemble N-bit output from individual bit results.
    RTLIL::Wire *last_wire = module->addWire(NEW_ID, wire_width);
    module->connect(last_wire, output_bits);

    // Collapse per-bit errors into a single 1-bit error flag.
    RTLIL::Wire *err_wire = module->addWire(NEW_ID, 1);
    if (wire_width == 1) {
        module->connect(err_wire, error_bits);
    } else {
        module->connect(err_wire, module->ReduceOr(NEW_ID, error_bits));
    }

    return {last_wire, err_wire};
}

void connectErrorSignal(RTLIL::Module *mod, const std::vector<RTLIL::Wire *> &error_signals,
                        const Config *cfg) {

    RTLIL::Wire *sink = nullptr;
    for (auto w : mod->wires()) {
        if (!w->port_output) {
            continue;
        }
        if (isTmrErrorOutWire(w, cfg)) {
            if (sink != nullptr) {
                log_error("Duplicate error sinks, only one allowed");
            }
            sink = w;
        }
    }
    if (sink == nullptr && cfg->autoErrorPort && !error_signals.empty()) {
        RTLIL::IdString port_name = mod->uniquify(tmrx_auto_error_port_name);
        RTLIL::Wire *new_port = mod->addWire(port_name, 1);
        new_port->port_output = true;
        mod->fixup_ports();
        log("  Auto-created error port '%s' in module '%s'\n", port_name.c_str(),
            mod->name.c_str());

        RTLIL::SigSpec aggregated = RTLIL::State::S0;
        for (auto s : error_signals) {
            aggregated = mod->Or(NEW_ID, aggregated, s);
        }
        mod->connect(new_port, aggregated);
        return;
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
