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

    enum class PortKind {
        Data,
        Clock,
        Reset,
        Error,
    };

    enum class PortShape {
        Shared,
        Triplicated,
    };

    struct TriplicatedSignals {
        RTLIL::SigSpec a;
        RTLIL::SigSpec b;
        RTLIL::SigSpec c;
    };

    struct ChildPortNames {
        PortShape shape;
        RTLIL::IdString a;
        RTLIL::IdString b;
        RTLIL::IdString c;
    };

    struct ResolvedSubmodule {
        RTLIL::Module *logical_module;
        RTLIL::Module *effective_module;
        const Config *child_cfg;
        bool was_expanded_with_triplicated_ports;
    };

    bool is_in_clk_net(const RTLIL::Wire *w) {
        return clk_net_wires.count(RTLIL::SigSpec(w)) != 0;
    }

    bool is_in_rst_net(const RTLIL::Wire *w) {
        return rst_net_wires.count(RTLIL::SigSpec(w)) != 0;
    }

    bool should_keep_wire_shared(const RTLIL::Wire *wire, const Config *cfg) {
        return is_tmr_error_out_wire(const_cast<RTLIL::Wire *>(wire), cfg) ||
               (cfg->preserve_module_ports && wire->port_input) ||
               (is_clk_wire(wire, cfg) && !cfg->expand_clock) ||
               (is_rst_wire(wire, cfg) && !cfg->expand_reset);
    }

    bool is_exposed_module_port(const RTLIL::Wire *wire) {
        return wire != nullptr && (wire->port_input || wire->port_output);
    }

    TriplicatedSignals derive_parent_signals(
        const RTLIL::SigSpec &sig_a,
        const dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> &wire_map) {
        TriplicatedSignals signals = {sig_a, sig_a, sig_a};

        for (auto &kv : wire_map) {
            signals.b.replace(kv.first, kv.second.first);
            signals.c.replace(kv.first, kv.second.second);
        }

        return signals;
    }

    bool parent_signals_are_shared(const TriplicatedSignals &signals) {
        return signals.a == signals.b && signals.a == signals.c;
    }

    PortKind classify_port_kind(const RTLIL::Wire *port_wire, const Config *child_cfg) {
        if (is_tmr_error_out_wire(const_cast<RTLIL::Wire *>(port_wire), child_cfg)) {
            return PortKind::Error;
        }
        if (is_clk_wire(port_wire, child_cfg)) {
            return PortKind::Clock;
        }
        if (is_rst_wire(port_wire, child_cfg)) {
            return PortKind::Reset;
        }
        return PortKind::Data;
    }

    ChildPortNames resolve_child_port_names(RTLIL::Module *effective_cell_mod,
                                            RTLIL::IdString logical_port,
                                            const Config *child_cfg) {
        RTLIL::IdString port_a = RTLIL::IdString(logical_port.str() + child_cfg->logic_path_1_suffix);
        RTLIL::IdString port_b = RTLIL::IdString(logical_port.str() + child_cfg->logic_path_2_suffix);
        RTLIL::IdString port_c = RTLIL::IdString(logical_port.str() + child_cfg->logic_path_3_suffix);

        bool has_base = is_exposed_module_port(effective_cell_mod->wire(logical_port));
        bool has_a = is_exposed_module_port(effective_cell_mod->wire(port_a));
        bool has_b = is_exposed_module_port(effective_cell_mod->wire(port_b));
        bool has_c = is_exposed_module_port(effective_cell_mod->wire(port_c));

        if (has_base && !has_a && !has_b && !has_c) {
            return {PortShape::Shared, logical_port, logical_port, logical_port};
        }

        if (!has_base && has_a && has_b && has_c) {
            return {PortShape::Triplicated, port_a, port_b, port_c};
        }

        if (!has_base && !has_a && !has_b && !has_c) {
            log_error("Submodule port '%s' not found on effective module '%s'.\n",
                      logical_port.c_str(), effective_cell_mod->name.c_str());
        }

        log_error("Unexpected mixed port expansion for '%s' on module '%s'.\n",
                  logical_port.c_str(), effective_cell_mod->name.c_str());
    }

    void connect_parent_destinations(RTLIL::Module *mod, const TriplicatedSignals &destinations,
                                     const RTLIL::SigSpec &source) {
        mod->connect(destinations.a, source);
        if (destinations.b != destinations.a) {
            mod->connect(destinations.b, source);
        }
        if (destinations.c != destinations.a && destinations.c != destinations.b) {
            mod->connect(destinations.c, source);
        }
    }

    ResolvedSubmodule resolve_submodule(RTLIL::Module *mod, RTLIL::Cell *cell,
                                        const ConfigManager *cfg_mgr) {
        RTLIL::Module *logical_module = mod->design->module(cell->type);
        if (logical_module == nullptr) {
            log_error("No submodule definition found for cell '%s' (type '%s').\n",
                      cell->name.c_str(), cell->type.c_str());
        }

        const Config *child_cfg = cfg_mgr->cfg(logical_module);
        RTLIL::Module *effective_module = logical_module;
        bool was_expanded_with_triplicated_ports = logical_module->has_attribute(ID(tmrx_impl_module));

        if (was_expanded_with_triplicated_ports) {
            RTLIL::IdString impl_name =
                RTLIL::IdString(logical_module->get_string_attribute(ID(tmrx_impl_module)));
            cell->type = impl_name;
            effective_module = mod->design->module(impl_name);
            if (effective_module == nullptr) {
                log_error("No remapped submodule implementation '%s' for cell '%s'.\n",
                          impl_name.c_str(), cell->name.c_str());
            }
        }

        return {logical_module, effective_module, child_cfg, was_expanded_with_triplicated_ports};
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


std::vector<RTLIL::Wire *> connect_submodule_ports(
    RTLIL::Module *mod, RTLIL::Cell *cell, RTLIL::Module *logical_cell_mod,
    RTLIL::Module *effective_cell_mod, const Config *child_cfg, const Config *parent_cfg,
    const dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> &wire_map) {
    std::vector<RTLIL::Wire *> error_signals;
    if (!is_proper_submodule(logical_cell_mod)) {
        return error_signals;
    }

    dict<RTLIL::IdString, RTLIL::SigSpec> orig_connections;
    for (auto &conn : cell->connections()) {
        orig_connections[conn.first] = conn.second;
    }

    cell->connections_.clear();

    for (auto &port : effective_cell_mod->ports) {
        RTLIL::Wire *port_wire = effective_cell_mod->wire(port);
        if (orig_connections.count(port) == 0 && is_tmr_error_out_wire(port_wire, child_cfg)) {
            RTLIL::Wire *err_wire = mod->addWire(NEW_ID, port_wire->width);
            cell->setPort(port, err_wire);
            error_signals.push_back(err_wire);
        }
    }

    for (auto &orig_conn : orig_connections) {
        RTLIL::IdString logical_port = orig_conn.first;
        RTLIL::Wire *logical_port_wire = logical_cell_mod->wire(logical_port);
        if (logical_port_wire == nullptr) {
            log_error("No logical port '%s' on submodule '%s'.\n", logical_port.c_str(),
                      logical_cell_mod->name.c_str());
        }

        PortKind port_kind = classify_port_kind(logical_port_wire, child_cfg);
        ChildPortNames child_ports = resolve_child_port_names(effective_cell_mod, logical_port, child_cfg);
        TriplicatedSignals parent_signals = derive_parent_signals(orig_conn.second, wire_map);
        bool parent_shared = parent_signals_are_shared(parent_signals);

        if (logical_port_wire->port_input && logical_port_wire->port_output) {
            log_error("Inout port '%s' on submodule '%s' is not supported.\n",
                      logical_port.c_str(), logical_cell_mod->name.c_str());
        }

        if (logical_port_wire->port_input) {
            if (child_ports.shape == PortShape::Shared) {
                if (parent_shared) {
                    cell->setPort(child_ports.a, parent_signals.a);
                } else {
                    auto [voted_signal, error_signal] =
                        insert_voter(mod, {parent_signals.a, parent_signals.b, parent_signals.c},
                                     parent_cfg);
                    error_signals.push_back(error_signal);
                    cell->setPort(child_ports.a, voted_signal);
                }
            } else {
                if (parent_shared) {
                    cell->setPort(child_ports.a, parent_signals.a);
                    cell->setPort(child_ports.b, parent_signals.a);
                    cell->setPort(child_ports.c, parent_signals.a);
                } else {
                    cell->setPort(child_ports.a, parent_signals.a);
                    cell->setPort(child_ports.b, parent_signals.b);
                    cell->setPort(child_ports.c, parent_signals.c);
                }
            }
            continue;
        }

        if (!logical_port_wire->port_output) {
            if (port_kind == PortKind::Error) {
                continue;
            }
            log_error("Port '%s' on submodule '%s' is neither input nor output.\n",
                      logical_port.c_str(), logical_cell_mod->name.c_str());
        }

        if (child_ports.shape == PortShape::Shared) {
            RTLIL::Wire *new_output = mod->addWire(NEW_ID, orig_conn.second.size());
            cell->setPort(child_ports.a, new_output);
            connect_parent_destinations(mod, parent_signals, new_output);
            continue;
        }

        if (!parent_shared) {
            cell->setPort(child_ports.a, parent_signals.a);
            cell->setPort(child_ports.b, parent_signals.b);
            cell->setPort(child_ports.c, parent_signals.c);
            continue;
        }

        RTLIL::Wire *out_a = mod->addWire(NEW_ID, orig_conn.second.size());
        RTLIL::Wire *out_b = mod->addWire(NEW_ID, orig_conn.second.size());
        RTLIL::Wire *out_c = mod->addWire(NEW_ID, orig_conn.second.size());
        cell->setPort(child_ports.a, out_a);
        cell->setPort(child_ports.b, out_b);
        cell->setPort(child_ports.c, out_c);

        auto [voted_signal, error_signal] = insert_voter(mod, {out_a, out_b, out_c}, parent_cfg);
        error_signals.push_back(error_signal);
        connect_parent_destinations(mod, parent_signals, voted_signal);
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
        if (should_keep_wire_shared(w, cfg)) {
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
        if (should_keep_wire_shared(w, cfg)) {
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

        ResolvedSubmodule submodule = resolve_submodule(mod, cell, cfg_mgr);

        log("    Connecting submodule '%s' (type '%s', preserve_ports=%s, expanded=%s)\n",
            cell->name.c_str(), cell->type.c_str(),
            submodule.child_cfg->preserve_module_ports ? "true" : "false",
            submodule.was_expanded_with_triplicated_ports ? "true" : "false");

        auto v_err_w = connect_submodule_ports(mod, cell, submodule.logical_module,
                                               submodule.effective_module, submodule.child_cfg,
                                               cfg, combined_wire_map);
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
