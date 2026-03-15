#include "tmrx_mod_expansion.h"
#include "kernel/rtlil.h"
#include "tmrx_utils.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {
namespace {

// Recursively clone and rename all proper submodule cells within `mod`,
// appending `suffix` to their type names.  Creates new uniquified module
// definitions in `design` as needed (skips creation if already present).
void uniquify_submodules_recursive(RTLIL::Module *mod, const std::string &suffix,
                                   RTLIL::Design *design) {
    std::vector<RTLIL::Cell *> cells(mod->cells().begin(), mod->cells().end());

    for (auto cell : cells) {
        RTLIL::Module *cell_mod = design->module(cell->type);
        if (cell_mod == nullptr || !is_proper_submodule(cell_mod)) {
            continue;
        }
        // Blackbox modules (standard cells, user IP blackboxes) are
        // independent per-instance by definition — multiple workers can share
        // the same blackbox type without needing uniquified copies.
        // Cloning them also risks breaking port connections when the original
        // was already processed by full_module_tmr_expansion.
        if (cell_mod->get_blackbox_attribute()) {
            continue;
        }

        RTLIL::IdString unique_name = RTLIL::IdString(cell->type.str() + suffix);

        if (design->module(unique_name) == nullptr) {
            RTLIL::Module *unique_mod = design->addModule(unique_name);
            cell_mod->cloneInto(unique_mod);
            unique_mod->name = unique_name;
            unique_mod->set_bool_attribute(ID(tmrx_is_proper_submodule), true);
            // cloneInto copies all attributes, including tmrx_impl_module if
            // the source was processed by TMRX. The clone is an independent
            // worker module — it must not carry tmrx_impl_module or the
            // cleanup loop in tmrx_pass will remove it from the design.
            unique_mod->attributes.erase(ID(tmrx_impl_module));

            // Recurse so deeper submodule levels are also uniquified.
            uniquify_submodules_recursive(unique_mod, suffix, design);
        }

        cell->type = unique_name;
    }
}

} // namespace

void full_module_tmr_expansion(RTLIL::Module *mod, const Config *cfg) {
    std::vector<RTLIL::Wire *> error_wires;

    RTLIL::IdString org_mod_name = mod->name;
    log("  Full Module TMR: creating 3-instance wrapper for '%s'\n", org_mod_name.c_str());
    std::string mod_name = mod->name.str() + "_tmrx_worker";

    mod->design->rename(mod, mod_name);
    RTLIL::Module *wrapper = mod->design->addModule(org_mod_name);
    wrapper->set_bool_attribute(ID(tmrx_is_proper_submodule));
    dict<RTLIL::Wire *, std::vector<RTLIL::Wire *>> wire_map;

    std::vector<std::string> suffixes =
        (cfg->preserve_module_ports
             ? std::vector<std::string>{""}
             : std::vector<std::string>{cfg->logic_path_1_suffix, cfg->logic_path_2_suffix,
                                        cfg->logic_path_3_suffix});

    for (size_t i = 0; i < 3; i++) {
        for (auto w : mod->wires()) {
            if (w->port_id == 0) {
                continue;
            }
            bool ignore_wire = (is_clk_wire(w, cfg) && !cfg->expand_clock) ||
                               (is_rst_wire(w, cfg) && !cfg->expand_reset) ||
                               is_tmr_error_out_wire(w);

            if ((cfg->preserve_module_ports || ignore_wire) && i > 0) {
                wire_map[w].push_back(wire_map.at(w).at(0));
                continue;
            }

            string new_wire_name = (w->name.str() + (ignore_wire ? "" : suffixes.at(i)));

            RTLIL::Wire *w_b = wrapper->addWire(new_wire_name, w->width);
            w_b->port_input = w->port_input;
            w_b->port_output = w->port_output;
            w_b->start_offset = w->start_offset;
            w_b->upto = w->upto;
            w_b->attributes = w->attributes;
            wire_map[w].push_back(w_b);
        }
    }
    wrapper->fixup_ports();

    std::vector<RTLIL::Cell *> duplicates;
    std::vector<dict<RTLIL::Wire *, RTLIL::Wire *>> cell_ports;

    for (size_t i = 0; i < 3; i++) {
        RTLIL::Cell *cell = wrapper->addCell(NEW_ID, mod_name);
        duplicates.push_back(cell);

        cell_ports.push_back({});

        for (auto w : mod->wires()) {
            if (w->port_id == 0) {
                continue;
            }
            RTLIL::Wire *w_con = wrapper->addWire(NEW_ID, w->width);
            cell_ports.at(i)[(w)] = w_con;
            cell->setPort(w->name, w_con);

            if (is_tmr_error_out_wire(w)) {
                error_wires.push_back(w_con);
            }
        }
    }

    if (cfg->tmr_mode_full_module_insert_voter_before_modules && !cfg->preserve_module_ports) {
        for (auto wm : wire_map) {
            if (!wm.first->port_input || (is_clk_wire(wm.first, cfg) && !cfg->expand_clock) ||
                (is_rst_wire(wm.first, cfg) && !cfg->expand_reset) ||
                (is_clk_wire(wm.first, cfg) && !cfg->tmr_mode_full_module_insert_voter_on_clock_nets) ||
                (is_rst_wire(wm.first, cfg) && !cfg->tmr_mode_full_module_insert_voter_on_reset_nets)) {
                continue;
            }

            std::vector<RTLIL::Wire *> voter_outs;

            for (size_t i = 0; i < 3; i++) {
                auto [v_out, err] = insert_voter(
                    wrapper, {wm.second.at(0), wm.second.at(1), wm.second.at(2)}, cfg);
                error_wires.push_back(err);
                voter_outs.push_back(v_out);
            }

            wire_map[wm.first] = voter_outs;
        }
    }

    if (cfg->tmr_mode_full_module_insert_voter_after_modules && !cfg->preserve_module_ports) {
        for (auto wm : wire_map) {
            if (!wm.first->port_output || is_tmr_error_out_wire(wm.first) ||
                (is_clk_wire(wm.first, cfg) && !cfg->expand_clock) ||
                (is_rst_wire(wm.first, cfg) && !cfg->expand_reset) ||
                (is_clk_wire(wm.first, cfg) && !cfg->tmr_mode_full_module_insert_voter_on_clock_nets) ||
                (is_rst_wire(wm.first, cfg) && !cfg->tmr_mode_full_module_insert_voter_on_reset_nets)) {
                continue;
            }

            std::vector<RTLIL::Wire *> voter_outs;

            for (size_t i = 0; i < 3; i++) {
                auto [v_out, err] =
                    insert_voter(wrapper,
                                 {cell_ports.at(0).at(wm.first), cell_ports.at(1).at(wm.first),
                                  cell_ports.at(2).at(wm.first)},
                                 cfg);
                error_wires.push_back(err);
                voter_outs.push_back(v_out);
            }

            cell_ports.at(0).at(wm.first) = voter_outs.at(0);
            cell_ports.at(1).at(wm.first) = voter_outs.at(1);
            cell_ports.at(2).at(wm.first) = voter_outs.at(2);
        }
    }

    if (cfg->preserve_module_ports || !cfg->expand_clock || !cfg->expand_reset) {
        for (auto wm : wire_map) {
            if (!wm.first->port_output) continue;
            if (is_tmr_error_out_wire(wm.first)) continue;
            if (!cfg->preserve_module_ports && is_clk_wire(wm.first, cfg) && cfg->expand_clock) continue;
            if (!cfg->preserve_module_ports && is_rst_wire(wm.first, cfg) && cfg->expand_reset) continue;
            if (!cfg->preserve_module_ports && !is_clk_wire(wm.first, cfg) && !is_rst_wire(wm.first, cfg)) continue;


                auto [v_out, err] =
                    insert_voter(wrapper,
                                 {cell_ports.at(0).at(wm.first), cell_ports.at(1).at(wm.first),
                                  cell_ports.at(2).at(wm.first)},
                                 cfg);
                error_wires.push_back(err);
                cell_ports.at(0).at(wm.first) = v_out;
                cell_ports.at(1).at(wm.first) = v_out;
                cell_ports.at(2).at(wm.first) = v_out;
        }
    }

    for (size_t i = 0; i < 3; i++) {
        for (auto wm : wire_map) {
            if (is_tmr_error_out_wire(wm.first)) {
                continue;
            }
            wrapper->connect(wm.second.at(i), cell_ports.at(i).at(wm.first));
        }
    }

    connect_error_signal(wrapper, error_wires);

    // Create one uniquified worker clone per TMR path and recursively uniquify
    // all proper submodule cells within each clone so that the three workers
    // are fully independent module hierarchies.
    std::vector<std::string> path_suffixes = {
        cfg->logic_path_1_suffix,
        cfg->logic_path_2_suffix,
        cfg->logic_path_3_suffix,
    };

    RTLIL::Design *design = mod->design;
    for (size_t i = 0; i < 3; i++) {
        RTLIL::IdString worker_name =
            RTLIL::IdString(mod->name.str() + path_suffixes[i]);

        RTLIL::Module *worker = design->addModule(worker_name);
        mod->cloneInto(worker);
        worker->name = worker_name;
        worker->set_bool_attribute(ID(tmrx_is_proper_submodule), true);

        uniquify_submodules_recursive(worker, path_suffixes[i], design);

        duplicates[i]->type = worker_name;
    }
    // mod (the template worker) is no longer referenced by any cell.
    // Remove it immediately: the _a/_b/_c workers hold the complete
    // implementation, so the template is redundant. Leaving it in the design
    // as an unreachable orphan causes `stat -top` to crash because stat
    // iterates design->selected_modules() but only builds mod_stat for
    // modules reachable from the top (std::out_of_range on missing keys).
    design->remove(mod);
}
}

YOSYS_NAMESPACE_END
