#include "tmrx_logic_expansion.h"
#include "kernel/rtlil.h"
#include "tmrx_utils.h"
#include "utils.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {
namespace {
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
        RTLIL::SigSpec sig;
        log("Port %s, cnt :%i error: %i\n", port.c_str(), orig_connections.count(port),
            is_tmr_error_out_wire(port_wire));
        if (orig_connections.count(port) == 0 && is_tmr_error_out_wire(port_wire)) {
            RTLIL::Wire *err_wire = mod->addWire(NEW_ID, port_wire->width);
            cell->setPort(port, err_wire);
            error_signals.push_back(err_wire);
        }

        log("Looking at port %s, sig: %s\n", port.c_str(), log_signal(sig));
    }

    for (auto &orig_conn : orig_connections) {
        RTLIL::IdString port = orig_conn.first;
        RTLIL::Wire *port_wire = cell_mod->wire(port);
        RTLIL::SigSpec sig = orig_conn.second;

        RTLIL::IdString port_a = RTLIL::IdString(port.str() + cell_cfg->logic_path_1_suffix);
        RTLIL::IdString port_b = RTLIL::IdString(port.str() + cell_cfg->logic_path_2_suffix);
        RTLIL::IdString port_c = RTLIL::IdString(port.str() + cell_cfg->logic_path_3_suffix);

        RTLIL::SigSpec sig_b = wire_map.at(sig).first;
        RTLIL::SigSpec sig_c = wire_map.at(sig).second;

        if (port_wire != nullptr && (is_tmr_error_out_wire(port_wire) ||
                                     (is_clk_wire(port_wire, cell_cfg) && !cell_cfg->expand_clock &&
                                      !mod_cfg->expand_clock) ||
                                     (is_rst_wire(port_wire, cell_cfg) && !cell_cfg->expand_reset &&
                                      !mod_cfg->expand_reset))) {
            port_a = RTLIL::IdString(port.str());
            port_b = RTLIL::IdString(port.str());
            port_c = RTLIL::IdString(port.str());
            sig_b = sig;
            sig_c = sig;
        }

        if (port_wire != nullptr && ((is_clk_wire(port_wire, cell_cfg) && !cell_cfg->expand_clock &&
                                      mod_cfg->expand_clock) ||
                                     (is_rst_wire(port_wire, cell_cfg) && !cell_cfg->expand_reset &&
                                      mod_cfg->expand_reset))) {
            auto [res_v, error_v] = insert_voter(mod, {sig, sig_b, sig_c}, mod->design);
            error_signals.push_back(error_v);

            port_a = RTLIL::IdString(port.str());
            port_b = RTLIL::IdString(port.str());
            port_c = RTLIL::IdString(port.str());

            sig = res_v;
            sig_b = res_v;
            sig_c = res_v;
        }

        cell->setPort(port_a, sig);
        cell->setPort(port_b, sig_b);
        cell->setPort(port_c, sig_c);
    }

    return error_signals;
}

std::vector<RTLIL::Wire *> connect_submodules_preserver_mod_ports(
    RTLIL::Module *mod, RTLIL::Cell *cell,
    dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> wire_map) {
    log_header(mod->design, "Connect sub pre por\n");
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
        RTLIL::SigSpec sig;
        log("Port %s, cnt :%i error: %i\n", port.c_str(), orig_connections.count(port),
            is_tmr_error_out_wire(port_wire));
        if (orig_connections.count(port) == 0 && is_tmr_error_out_wire(port_wire)) {
            RTLIL::Wire *err_wire = mod->addWire(NEW_ID, port_wire->width);
            cell->setPort(port, err_wire);
            error_signals.push_back(err_wire);
        }

        log("Looking at port %s, sig: %s\n", port.c_str(), log_signal(sig));
    }

    for (auto port : input_ports) {
        RTLIL::SigSpec sig_a = cell->getPort(port);

        RTLIL::SigSpec sig_b = wire_map.at(sig_a).first;
        RTLIL::SigSpec sig_c = wire_map.at(sig_a).second;
        std::vector<RTLIL::SigSpec> inputs = {sig_a, sig_b, sig_c};

        // log("Port %s sigs %i", port.c_str(), sig_a==sig_b);
        if ((sig_a == sig_b) && (sig_a == sig_c) && (sig_b == sig_c)) {
            continue;
        }

        std::pair<RTLIL::Wire *, RTLIL::Wire *> res = insert_voter(mod, inputs, design);
        error_signals.push_back(res.second);
        cell->setPort(port, res.first);
    }

    for (auto port : output_ports) {
        RTLIL::SigSpec sig_a = cell->getPort(port);

        RTLIL::SigSpec sig_b = wire_map.at(sig_a).first;
        RTLIL::SigSpec sig_c = wire_map.at(sig_a).second;

        RTLIL::Wire *new_output = mod->addWire(NEW_ID, sig_a.size());
        cell->setPort(port, new_output);
        mod->connect(new_output, sig_a);
        mod->connect(new_output, sig_b);
        mod->connect(new_output, sig_c);
    }

    return error_signals;
}

std::vector<RTLIL::Wire *> insert_voter_after_ff(RTLIL::Module *mod,
                                                 dict<Cell *, std::pair<Cell *, Cell *>> ff_map) {
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
                    insert_voter(mod, intermediate_wires, mod->design);
                mod->connect(res_wires.first, original_signals.at(i));

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
        outputs.first->port_output = false;

        std::vector<RTLIL::SigSpec> out_sigs = {};

        outputs.first->port_output = false;
        out_sigs.push_back(outputs.first);

        outputs.second.first->port_output = false;
        out_sigs.push_back(outputs.second.first);

        outputs.second.second->port_output = false;
        out_sigs.push_back(outputs.second.second);

        std::pair<RTLIL::Wire *, RTLIL::Wire *> res_wires =
            insert_voter(mod, out_sigs, mod->design);
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
    log_header(mod->design, "Renaming wires with suffix %s\n", suffix.c_str());
    for (auto w : wires) {
        if (is_tmr_error_out_wire(w) || (cfg->preserve_module_ports && w->port_input) ||
            (is_clk_wire(w, cfg) && !cfg->expand_clock) ||
            (is_rst_wire(w, cfg) && !cfg->expand_reset)) {
            continue;
        }
        mod->rename(w, mod->uniquify(w->name.str() + suffix));
    }

    for (auto c : cells) {
        if (is_proper_submodule(c->module->design->module(c->type))) {
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

        log("Wire %s is clock: %i\n", w->name.c_str(), is_clk_wire(w, cfg));
        // TODO: verify if this actually works, fix no clk/rst expansion
        if ((cfg->preserve_module_ports && w->port_input) ||
            (is_clk_wire(w, cfg) && !cfg->expand_clock) ||
            (is_rst_wire(w, cfg) && !cfg->expand_reset)) {
            wire_map[w] = w;
            continue;
        }

        // TODO: move attr to header
        if (is_tmr_error_out_wire(w)) {
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
        if (is_proper_submodule(c->module->design->module(c->type))) {
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

} // namespace
void logic_tmr_expansion(RTLIL::Module *mod, const ConfigManager *cfg_mgr) {
    const Config *cfg = cfg_mgr->cfg(mod);

    std::vector<RTLIL::Wire *> original_wires(mod->wires().begin(), mod->wires().end());
    std::vector<RTLIL::Cell *> original_cells(mod->cells().begin(), mod->cells().end());
    std::vector<RTLIL::SigSig> original_connections(mod->connections().begin(),
                                                    mod->connections().end());
    std::vector<RTLIL::Wire *> error_wires;

    log_header(mod->design, "Logic TMR expansion");
    auto [wiremap_b, outputmap_b, flipflopmap_b] = insert_duplicate_logic(
        mod, original_wires, original_cells, original_connections, cfg->logic_path_2_suffix, cfg);
    auto [wiremap_c, outputmap_c, flipflopmap_c] = insert_duplicate_logic(
        mod, original_wires, original_cells, original_connections, cfg->logic_path_3_suffix, cfg);

    dict<RTLIL::Wire *, std::pair<RTLIL::Wire *, RTLIL::Wire *>> combined_output_map =
        zip_dicts(outputmap_b, outputmap_c);
    dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> combined_wire_map =
        zip_dicts(wiremap_b, wiremap_c);
    dict<RTLIL::Cell *, std::pair<RTLIL::Cell *, RTLIL::Cell *>> combined_ff_map =
        zip_dicts(flipflopmap_b, flipflopmap_c);

    for (auto cell : original_cells) {
        RTLIL::Module *cell_mod = mod->design->module(cell->type);
        if (!is_proper_submodule(cell_mod)) {
            continue;
        }
        const Config *cell_cfg = cfg_mgr->cfg(cell_mod);
        std::vector<RTLIL::Wire *> v_err_w;

        log_header(mod->design, "Connecting submodules\n");
        if (cell_cfg->preserve_module_ports) {
            v_err_w = connect_submodules_preserver_mod_ports(mod, cell, combined_wire_map);
        } else {
            v_err_w = connect_submodules_mod_ports(mod, cell, cell_cfg, cfg, combined_wire_map);
        }
        error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
    }

    rename_wires_and_cells(mod, original_wires, original_cells, cfg->logic_path_1_suffix, cfg);

    if (cfg->preserve_module_ports) {
        auto v_err_w = insert_output_voters(mod, combined_output_map, cfg);
        error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
    }

    // insert ff voters

    if (cfg->insert_voter_before_ff) {
        log_error("Insert before ff not yet implemented");
    }

    if (cfg->insert_voter_after_ff) {
        auto v_err_w = insert_voter_after_ff(mod, combined_ff_map);
        error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
    }

    connect_error_signal(mod, error_wires);
}

}

YOSYS_NAMESPACE_END
