#include "tmrx_logic_expansion.h"
#include "config_manager.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "tmrx_utils.h"
#include "utils.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {
namespace {


    Yosys::pool<RTLIL::SigSpec> clk_net_wires;
    Yosys::pool<RTLIL::SigSpec> rst_net_wires;

    bool is_in_clk_net(const RTLIL::Wire *w) {
        return clk_net_wires.count(RTLIL::SigSpec(w)) != 0;
    }

    bool is_in_rst_net(const RTLIL::Wire *w) {
        return rst_net_wires.count(RTLIL::SigSpec(w)) != 0;
    }

    void build_clk_net(RTLIL::Module *mod, const ConfigManager *cfg_mgr){
        clk_net_wires.clear();

        const Config *cfg = cfg_mgr->cfg(mod);
        RTLIL::Design *design = mod->design;

        // Add wires connected to clock ports of the current module
        for (auto wire : mod->wires()) {
            if ((wire->port_input || wire->port_output) && is_clk_wire(wire, cfg)) {
                clk_net_wires.insert(RTLIL::SigSpec(wire));
            }
        }

        // Add wires connected to clock ports of cells (submodules)
        for (auto cell : mod->cells()) {
            RTLIL::Module *cell_mod = design->module(cell->type);
            if (cell_mod == nullptr || !is_proper_submodule(cell_mod)) {
                continue;
            }

            const Config *cell_cfg = cfg_mgr->cfg(cell_mod);

            for (auto &conn : cell->connections()) {
                RTLIL::IdString port_name = conn.first;
                RTLIL::Wire *port_wire = cell_mod->wire(port_name);

                if (port_wire != nullptr && is_clk_wire(port_wire, cell_cfg)) {
                    clk_net_wires.insert(conn.second);
                }
            }
        }
    }

    void build_rst_net(RTLIL::Module *mod, const ConfigManager *cfg_mgr){
        rst_net_wires.clear();

        const Config *cfg = cfg_mgr->cfg(mod);
        RTLIL::Design *design = mod->design;

        // Add wires connected to reset ports of the current module
        for (auto wire : mod->wires()) {
            if ((wire->port_input || wire->port_output) && is_rst_wire(wire, cfg)) {
                rst_net_wires.insert(RTLIL::SigSpec(wire));
            }
        }

        // Add wires connected to reset ports of cells (submodules)
        for (auto cell : mod->cells()) {
            RTLIL::Module *cell_mod = design->module(cell->type);
            if (cell_mod == nullptr || !is_proper_submodule(cell_mod)) {
                continue;
            }

            const Config *cell_cfg = cfg_mgr->cfg(cell_mod);

            for (auto &conn : cell->connections()) {
                RTLIL::IdString port_name = conn.first;
                RTLIL::Wire *port_wire = cell_mod->wire(port_name);

                if (port_wire != nullptr && is_rst_wire(port_wire, cell_cfg)) {
                    rst_net_wires.insert(conn.second);
                }
            }
        }
    }


std::vector<RTLIL::Wire *> connect_submodules_mod_ports(
    RTLIL::Module *mod, RTLIL::Cell *cell, const Config *cell_cfg, const Config *mod_cfg,
    dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> wire_map) {

    std::vector<RTLIL::Wire *> error_signals;
    RTLIL::Design *design = mod->design;

    if (!is_proper_submodule(design->module(cell->type))) {
        return error_signals;
    }

    // const RTLIL::Module *cell_mod = design->module(cell->type);

    dict<RTLIL::IdString, RTLIL::SigSpec> orig_connections;
    for (auto &conn : cell->connections()) {
        orig_connections[conn.first] = conn.second;
    }

    cell->connections_.clear();

    // TODO: handle  signal

    RTLIL::Module *cell_mod = design->module(cell->type);

    if (cell_mod == nullptr) {
        log_error("No cell mod\n");
    }

    for (auto &port : cell_mod->ports) {
        RTLIL::Wire *port_wire = cell_mod->wire(port);
        if (orig_connections.count(port) == 0 && is_tmr_error_out_wire(port_wire, cell_cfg)) {
            RTLIL::Wire *err_wire = mod->addWire(NEW_ID, port_wire->width);
            cell->setPort(port, err_wire);
            error_signals.push_back(err_wire);
        }
    }

    for (auto &orig_conn : orig_connections) {
        RTLIL::IdString port = orig_conn.first;
        RTLIL::Wire *port_wire = cell_mod->wire(port);
        RTLIL::SigSpec sig = orig_conn.second;

        RTLIL::IdString port_a = RTLIL::IdString(port.str() + cell_cfg->logic_path_1_suffix);
        RTLIL::IdString port_b = RTLIL::IdString(port.str() + cell_cfg->logic_path_2_suffix);
        RTLIL::IdString port_c = RTLIL::IdString(port.str() + cell_cfg->logic_path_3_suffix);

        // Build B/C copies using replace so bit-slice connections
        // (e.g. bus[3]) are handled even though wire_map keys are full wires.
        RTLIL::SigSpec sig_b = sig, sig_c = sig;
        for (auto &kv : wire_map) {
            sig_b.replace(kv.first, kv.second.first);
            sig_c.replace(kv.first, kv.second.second);
        }

        if (port_wire != nullptr) {
                    bool is_clk = is_clk_wire(port_wire, cell_cfg);
                    bool is_rst = is_rst_wire(port_wire, cell_cfg);
                    bool is_err = is_tmr_error_out_wire(port_wire, cell_cfg);

                    bool cell_no_exp_clk = is_clk && !cell_cfg->expand_clock;
                    bool cell_no_exp_rst = is_rst && !cell_cfg->expand_reset;
                    bool mod_exp_clk = is_clk && mod_cfg->expand_clock;
                    bool mod_exp_rst = is_rst && mod_cfg->expand_reset;
                    bool mod_no_exp_clk = is_clk && !mod_cfg->expand_clock;
                    bool mod_no_exp_rst = is_rst && !mod_cfg->expand_reset;

                    // Case 1: Neither module expands, or it's an error wire
                    if (is_err || (cell_no_exp_clk && mod_no_exp_clk) || (cell_no_exp_rst && mod_no_exp_rst)) {
                        port_a = RTLIL::IdString(port.str());
                        port_b = RTLIL::IdString(port.str());
                        port_c = RTLIL::IdString(port.str());
                        // For output clock/rst ports the submodule drives a single
                        // signal, but the parent may have triplicated the receiving
                        // wire (e.g. nclk_b, nclk_c). Fan out to all copies so they
                        // are not left undriven.
                        if (!is_err && port_wire->port_output) {
                            mod->connect(sig_b, sig);
                            mod->connect(sig_c, sig);
                        }
                        sig_b = sig;
                        sig_c = sig;
                    }
                    // Case 2: Parent module expands, but submodule does not
                    else if ((cell_no_exp_clk && mod_exp_clk) || (cell_no_exp_rst && mod_exp_rst)) {
                        port_a = RTLIL::IdString(port.str());
                        port_b = RTLIL::IdString(port.str());
                        port_c = RTLIL::IdString(port.str());

                        if (port_wire->port_input) {
                            // Reduce 3 parent signals to 1 input for the submodule
                            auto [res_v, error_v] = insert_voter(mod, {sig, sig_b, sig_c}, mod_cfg);
                            error_signals.push_back(error_v);
                            sig = res_v;
                            sig_b = res_v;
                            sig_c = res_v;
                        } else if (port_wire->port_output) {
                            // Fanout 1 submodule output to 3 parent signals
                            mod->connect(sig_b, sig);
                            mod->connect(sig_c, sig);
                            sig_b = sig;
                            sig_c = sig;
                        }
                    }
                }

        cell->setPort(port_a, sig);
        cell->setPort(port_b, sig_b);
        cell->setPort(port_c, sig_c);
    }

    return error_signals;
}

std::vector<RTLIL::Wire *> connect_submodules_preserver_mod_ports(
    RTLIL::Module *mod, RTLIL::Cell *cell,
    dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> wire_map, const Config *cfg) {
    std::vector<RTLIL::Wire *> error_signals;
    RTLIL::Design *design = mod->design;

    if (!is_proper_submodule(design->module(cell->type))) {
        return error_signals;
    }

    auto [input_ports, output_ports] = get_port_names(cell, design);

    // TODO: Optimization do not insert voter if input into submodule is
    // input into worker module
    //
    //

    RTLIL::Module *cell_mod = design->module(cell->type);
    if (cell_mod == nullptr) {
        log_error("No cell mod\n");
    }

    dict<RTLIL::IdString, RTLIL::SigSpec> orig_connections;
    for (auto &conn : cell->connections()) {
        orig_connections[conn.first] = conn.second;
    }

    for (auto &port : cell_mod->ports) {
        RTLIL::Wire *port_wire = cell_mod->wire(port);
        if (orig_connections.count(port) == 0 && is_tmr_error_out_wire(port_wire, cfg)) {
            RTLIL::Wire *err_wire = mod->addWire(NEW_ID, port_wire->width);
            cell->setPort(port, err_wire);
            error_signals.push_back(err_wire);
        }
    }

    for (auto port : input_ports) {
        RTLIL::SigSpec sig_a = cell->getPort(port);

        RTLIL::SigSpec sig_b = sig_a, sig_c = sig_a;
        for (auto &kv : wire_map) {
            sig_b.replace(kv.first, kv.second.first);
            sig_c.replace(kv.first, kv.second.second);
        }
        std::vector<RTLIL::SigSpec> inputs = {sig_a, sig_b, sig_c};

        // log("Port %s sigs %i", port.c_str(), sig_a==sig_b);
        if ((sig_a == sig_b) && (sig_a == sig_c) && (sig_b == sig_c)) {
            continue;
        }

        std::pair<RTLIL::Wire *, RTLIL::Wire *> res = insert_voter(mod, inputs, cfg);
        error_signals.push_back(res.second);
        cell->setPort(port, res.first);
    }

    for (auto port : output_ports) {
        RTLIL::SigSpec sig_a = cell->getPort(port);

        RTLIL::SigSpec sig_b = sig_a, sig_c = sig_a;
        for (auto &kv : wire_map) {
            sig_b.replace(kv.first, kv.second.first);
            sig_c.replace(kv.first, kv.second.second);
        }

        RTLIL::Wire *new_output = mod->addWire(NEW_ID, sig_a.size());
        cell->setPort(port, new_output);
        mod->connect(sig_a, new_output);
        mod->connect(sig_b, new_output);
        mod->connect(sig_c, new_output);
    }

    return error_signals;
}

std::vector<RTLIL::Wire *> insert_voter_after_ff(RTLIL::Module *mod,
                                                 dict<Cell *, std::pair<Cell *, Cell *>> ff_map, const Config *cfg) {
    std::vector<RTLIL::Wire *> error_signals;

    for (auto flip_flops : ff_map) {

        auto [input_ports, output_ports] = get_port_names(flip_flops.first, mod->design);

        if (output_ports.empty()) {
            log("Cell Type: %s\n", flip_flops.first->type.str().c_str());
            log_error("Flip Flop without output found");
        }

        for (auto port : output_ports) {
            std::vector<RTLIL::SigSpec> intermediate_wires;
            std::vector<RTLIL::SigSpec> original_signals;

            for (auto ff : {flip_flops.first, flip_flops.second.first, flip_flops.second.second}) {
                RTLIL::SigSpec out_signal = ff->getPort(port);
                RTLIL::Wire *intermediate_wire = mod->addWire(NEW_ID, out_signal.size());

                ff->setPort(port, intermediate_wire);
                intermediate_wires.push_back(intermediate_wire);
                original_signals.push_back(out_signal);
            }

            for (size_t i = 0; i < 3; i++) {
                std::pair<RTLIL::Wire *, RTLIL::Wire *> res_wires =
                    insert_voter(mod, intermediate_wires, cfg);
                mod->connect(original_signals.at(i), res_wires.first);

                error_signals.push_back(res_wires.second);
            }
        }
    }

    return error_signals;
}

std::vector<RTLIL::Wire *>
insert_output_voters(RTLIL::Module *mod,
                     dict<RTLIL::Wire *, std::pair<RTLIL::Wire *, RTLIL::Wire *>> out_map,
                     const Config *cfg) {
    std::vector<RTLIL::Wire *> error_signals;
    for (auto outputs : out_map) {
        if (!cfg->preserve_module_ports && is_in_clk_net(outputs.first) && cfg->expand_clock) continue;
        if (!cfg->preserve_module_ports && is_in_rst_net(outputs.first) && cfg->expand_reset) continue;
        if (!cfg->preserve_module_ports && !is_in_clk_net(outputs.first) && !is_in_rst_net(outputs.first)) continue;

        outputs.first->port_output = false;

        std::vector<RTLIL::SigSpec> out_sigs = {};

        outputs.first->port_output = false;
        out_sigs.push_back(outputs.first);

        outputs.second.first->port_output = false;
        out_sigs.push_back(outputs.second.first);

        outputs.second.second->port_output = false;
        out_sigs.push_back(outputs.second.second);

        // if(out_sigs.at(0) == out_sigs.at(1) && out_sigs.at(0) == out_sigs.at(2)) continue;

        std::pair<RTLIL::Wire *, RTLIL::Wire *> res_wires =
            insert_voter(mod, out_sigs, cfg);
        error_signals.push_back(res_wires.second);

        res_wires.first->port_output = true;
        mod->rename(res_wires.first,
                    mod->uniquify(outputs.first->name.str().substr(
                        0, outputs.first->name.str().size() - (cfg->logic_path_1_suffix.size()))));
    }

    return error_signals;
}

void rename_wires_and_cells(RTLIL::Module *mod, std::vector<RTLIL::Wire *> wires,
                            std::vector<RTLIL::Cell *> cells, std::string suffix,
                            const Config *cfg) {
    for (auto w : wires) {
        // Skip renaming for clock/reset wires when not expanding them (both inputs AND outputs)
        if (is_tmr_error_out_wire(w, cfg) || (cfg->preserve_module_ports && w->port_input) ||
            (is_clk_wire(w,cfg) && !cfg->expand_clock) ||
            (is_rst_wire(w,cfg) && !cfg->expand_reset)) {
            continue;
        }
        mod->rename(w, mod->uniquify(w->name.str() + suffix));
    }

    for (auto c : cells) {
        RTLIL::Module *cell_mod = c->module->design->module(c->type);
        if (is_proper_submodule(cell_mod) && !cell_mod->get_blackbox_attribute()) {
            continue;
        }

        mod->rename(c, mod->uniquify(c->name.str() + suffix));
    }
}

std::tuple<dict<RTLIL::SigSpec, RTLIL::SigSpec>, dict<RTLIL::Wire *, RTLIL::Wire *>,
           dict<RTLIL::Cell *, RTLIL::Cell *>>
insert_duplicate_logic(RTLIL::Module *mod, std::vector<RTLIL::Wire *> wires,
                       std::vector<RTLIL::Cell *> cells, std::vector<RTLIL::SigSig> connections,
                       std::string suffix, const Config *cfg) {
    dict<RTLIL::SigSpec, RTLIL::SigSpec> wire_map;
    dict<RTLIL::Wire *, RTLIL::Wire *> output_map;
    dict<RTLIL::Cell *, RTLIL::Cell *> flip_flop_map;

    for (auto w : wires) {

        // Skip duplication for clock/reset wires when not expanding them
        // This applies to BOTH input and output clock/reset ports
        // Use is_clk_wire/is_rst_wire (same as rename_wires_and_cells) for consistency
        if ((cfg->preserve_module_ports && w->port_input) ||
            (is_clk_wire(w, cfg) && !cfg->expand_clock) ||
            (is_rst_wire(w, cfg) && !cfg->expand_reset)) {
            wire_map[w] = w;
            continue;
        }

        // TODO: move attr to header
        if (is_tmr_error_out_wire(w, cfg)) {
            wire_map[w] = w;
            continue;
        }

        RTLIL::Wire *w_b = mod->addWire(mod->uniquify(w->name.str() + suffix), w->width);
        w_b->port_input = w->port_input;
        w_b->port_output = w->port_output;
        w_b->start_offset = w->start_offset;
        w_b->upto = w->upto;
        w_b->attributes = w->attributes;

        wire_map[w] = w_b;

        if (w->port_output) {
            output_map[w] = w_b;
        }
    }

    for (auto c : cells) {
        RTLIL::Module *cell_mod = c->module->design->module(c->type);
        if (is_proper_submodule(cell_mod) && !cell_mod->get_blackbox_attribute()) {
            continue;
        }

        RTLIL::Cell *c_b = mod->addCell(mod->uniquify(c->name.str() + suffix), c->type);

        // log("Looking at cell %u\n",
        // (is_flip_flop(c, worker, &known_ff_cell_names)));

        if (is_flip_flop(c, mod, cfg)) {
            // TODO: fix this
            flip_flop_map[c] = c_b;
        }

        c_b->parameters = c->parameters;
        c_b->attributes = c->attributes;

        for (auto &connection : c->connections()) {
            RTLIL::SigSpec sig = connection.second;

            for (auto &it : wire_map) {
                sig.replace(it.first, it.second);
            }

            c_b->setPort(connection.first, sig);
        }
    }

    // TODO: imprive this
    for (auto conn : connections) {
        RTLIL::SigSpec first = conn.first;
        RTLIL::SigSpec second = conn.second;

        for (auto &w : wire_map) {
            first.replace(w.first, w.second);
            second.replace(w.first, w.second);
        }

        mod->connect(first, second);
    }

    return {wire_map, output_map, flip_flop_map};
}

}
void logic_tmr_expansion(RTLIL::Module *mod, const ConfigManager *cfg_mgr,
                         const Config *cfg_override) {
    const Config *cfg = cfg_override ? cfg_override : cfg_mgr->cfg(mod);

    std::vector<RTLIL::Wire *> original_wires(mod->wires().begin(), mod->wires().end());
    std::vector<RTLIL::Cell *> original_cells(mod->cells().begin(), mod->cells().end());
    std::vector<RTLIL::SigSig> original_connections(mod->connections().begin(),
                                                    mod->connections().end());
    std::vector<RTLIL::Wire *> error_wires;

    log("  Logic TMR: expanding '%s' (%zu wire(s), %zu cell(s))\n",
        mod->name.c_str(), original_wires.size(), original_cells.size());

    log("  [1/6] Building clock/reset nets\n");
    build_clk_net(mod, cfg_mgr);
    build_rst_net(mod, cfg_mgr);

    log("  [2/6] Duplicating logic (paths B and C)\n");
    auto [wiremap_b, outputmap_b, flipflopmap_b] = insert_duplicate_logic(
        mod, original_wires, original_cells, original_connections, cfg->logic_path_2_suffix, cfg);
    auto [wiremap_c, outputmap_c, flipflopmap_c] = insert_duplicate_logic(
        mod, original_wires, original_cells, original_connections, cfg->logic_path_3_suffix, cfg);

    build_clk_net(mod, cfg_mgr);
    build_rst_net(mod, cfg_mgr);

    dict<RTLIL::Wire *, std::pair<RTLIL::Wire *, RTLIL::Wire *>> combined_output_map =
        zip_dicts(outputmap_b, outputmap_c);
    dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> combined_wire_map =
        zip_dicts(wiremap_b, wiremap_c);
    dict<RTLIL::Cell *, std::pair<RTLIL::Cell *, RTLIL::Cell *>> combined_ff_map =
        zip_dicts(flipflopmap_b, flipflopmap_c);

    log("  [3/6] Connecting submodule ports\n");
    for (auto cell : original_cells) {
        RTLIL::Module *cell_mod = mod->design->module(cell->type);
        // Blackbox cells (standard cells, liberty cells) are duplicated like
        // primitive cells in insert_duplicate_logic; they must not be treated
        // as proper submodules here or voters get inserted on every port.
        if (!is_proper_submodule(cell_mod) || cell_mod->get_blackbox_attribute()) {
            continue;
        }

        // Fetch cell_cfg from the ORIGINAL module before any type remapping,
        // so preserve_module_ports and other settings are correct.
        const Config *cell_cfg = cfg_mgr->cfg(cell_mod);

        // If this submodule was expanded with preserve_module_ports=false a
        // _tmrx_impl copy was created with triplicated ports. Remap the cell
        // to that impl so the parent's triplicated wires connect correctly.
        bool was_expanded_with_triplicated_ports = cell_mod->has_attribute(ID(tmrx_impl_module));
        if (was_expanded_with_triplicated_ports) {
            RTLIL::IdString impl_name = RTLIL::IdString(
                cell_mod->get_string_attribute(ID(tmrx_impl_module)));
            cell->type = impl_name;
            cell_mod = mod->design->module(impl_name);
        }

        std::vector<RTLIL::Wire *> v_err_w;

        // Use the voter-at-boundary path when:
        // - preserve_module_ports=true (ports kept as-is), OR
        // - the module was NOT expanded with triplicated ports (no _tmrx_impl):
        //   this covers tmr_mode=None, blackboxes skipped by the main loop,
        //   and any other case where ports were not triplicated.
        bool use_preserve_path =
            cell_cfg->preserve_module_ports || !was_expanded_with_triplicated_ports;
        log("    Connecting submodule '%s' (type '%s', preserve_ports=%s, expanded=%s)\n",
            cell->name.c_str(), cell->type.c_str(),
            cell_cfg->preserve_module_ports ? "true" : "false",
            was_expanded_with_triplicated_ports ? "true" : "false");
        if (use_preserve_path) {
            v_err_w = connect_submodules_preserver_mod_ports(mod, cell, combined_wire_map, cfg);
        } else {
            v_err_w = connect_submodules_mod_ports(mod, cell, cell_cfg, cfg, combined_wire_map);
        }
        error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
    }

    log("  [4/6] Renaming path-A wires/cells\n");
    rename_wires_and_cells(mod, original_wires, original_cells, cfg->logic_path_1_suffix, cfg);

    if (cfg->insert_voter_before_ff) {
        log_error("Insert before ff not yet implemented");
    }

    if (cfg->insert_voter_after_ff) {
        log("  [5/6] Inserting voters after %zu flip-flop(s)\n", combined_ff_map.size());
        auto v_err_w = insert_voter_after_ff(mod, combined_ff_map, cfg);
        error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
    }

    build_clk_net(mod, cfg_mgr);
    build_rst_net(mod, cfg_mgr);

    log("  [6/6] Inserting output voters / connecting error signal\n");
    if (cfg->preserve_module_ports || !cfg->expand_clock || !cfg->expand_reset) {
        auto v_err_w = insert_output_voters(mod, combined_output_map, cfg);
        error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
    }

    connect_error_signal(mod, error_wires, cfg);
}

}

YOSYS_NAMESPACE_END
