#include "tmrx_mod_expansion.h"
#include "kernel/rtlil.h"
#include "tmrx_utils.h"

YOSYS_NAMESPACE_BEGIN
namespace TMRX {
namespace {}

void full_module_tmr_expansion(RTLIL::Module *mod, const Config *cfg) {
    std::vector<RTLIL::Wire *> error_wires;

    RTLIL::IdString org_mod_name = mod->name;
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
                (is_rst_wire(wm.first, cfg) && !cfg->expand_reset)) {
                continue;
            }

            std::vector<RTLIL::Wire *> voter_outs;

            for (size_t i = 0; i < 3; i++) {
                auto [v_out, err] = insert_voter(
                    wrapper, {wm.second.at(0), wm.second.at(1), wm.second.at(2)}, wrapper->design);
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
                (is_rst_wire(wm.first, cfg) && !cfg->expand_reset)) {
                continue;
            }

            std::vector<RTLIL::Wire *> voter_outs;

            for (size_t i = 0; i < 3; i++) {
                auto [v_out, err] =
                    insert_voter(wrapper,
                                 {cell_ports.at(0).at(wm.first), cell_ports.at(1).at(wm.first),
                                  cell_ports.at(2).at(wm.first)},
                                 wrapper->design);
                error_wires.push_back(err);
                voter_outs.push_back(v_out);
            }

            cell_ports.at(0).at(wm.first) = voter_outs.at(0);
            cell_ports.at(1).at(wm.first) = voter_outs.at(1);
            cell_ports.at(2).at(wm.first) = voter_outs.at(2);
        }
    }

    if (cfg->preserve_module_ports) {
        for (auto wm : wire_map) {
            if (wm.first->port_output) {

                if (is_tmr_error_out_wire(wm.first)) {
                    continue;
                }

                auto [v_out, err] =
                    insert_voter(wrapper,
                                 {cell_ports.at(0).at(wm.first), cell_ports.at(1).at(wm.first),
                                  cell_ports.at(2).at(wm.first)},
                                 wrapper->design);
                error_wires.push_back(err);
                cell_ports.at(0).at(wm.first) = v_out;
                cell_ports.at(1).at(wm.first) = v_out;
                cell_ports.at(2).at(wm.first) = v_out;
            }
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
}
} // namespace TMRX

YOSYS_NAMESPACE_END
