#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include "tmrx.h"
#include "utils.h"
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct TmrxPass : public Pass {
  private:

  public:
    TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}

    bool is_proper_submodule(RTLIL::Module *mod) {
      // TODO: find alternative
      if ((mod != nullptr) && mod->has_attribute(ID(tmrx_is_proper_submodule))) {
        return mod->get_bool_attribute(ID(tmrx_is_proper_submodule));
      }

      return false;
    }


    bool is_flip_flop(const RTLIL::Cell *cell, const RTLIL::Module *module, const Config *cfg) {

      if(cfg->excluded_ff_cells.count(cell->type) != 0){
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

    std::pair<std::vector<RTLIL::IdString>, std::vector<RTLIL::IdString>>   get_port_names(const RTLIL::Cell *cell, const RTLIL::Design *design) {
      std::vector<RTLIL::IdString> outputs = {};
      std::vector<RTLIL::IdString> inputs = {};

      const RTLIL::Module *cell_mod = design->module(cell->type);
      if (cell_mod) {
          const_cast<RTLIL::Module*>(cell_mod)->fixup_ports();
        for (auto &conn : cell->connections()) {
          const RTLIL::Wire *wire = cell_mod->wire(conn.first);
          if (wire && wire->port_output) {
            outputs.push_back(conn.first);
          }

          if (wire && wire->port_input){
              inputs.push_back(conn.first);
          }
        }
      } else {
        for (auto &conn : cell->connections()) {
          if (cell->output(conn.first)) {
            outputs.push_back(conn.first);
          }
          if( cell->input(conn.first)){
              inputs.push_back(conn.first);
          }
        }
      }

      return {inputs,outputs};
    }

    RTLIL::IdString createVoterCell(RTLIL::Design *design, size_t wire_width) {
      // TODO: make voter behaviour more predictable
      RTLIL::IdString voter_name =
          "\\tmrx_simple_voter_" + std::to_string(wire_width);

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

    bool is_tmr_error_out_wire(RTLIL::Wire* w){
        return (w->has_attribute(ID(tmrx_error_sink)));
    }

    // TODO: remove mod and design
    std::pair<RTLIL::Wire *, RTLIL::Wire *>
    insert_voter(RTLIL::Module *module, std::vector<RTLIL::SigSpec> inputs,
                 RTLIL::Design *design) {
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


    std::tuple<dict<RTLIL::SigSpec, RTLIL::SigSpec> , dict<RTLIL::Wire *, RTLIL::Wire *>, dict<RTLIL::Cell*,RTLIL::Cell*>> insert_duplicate_logic(RTLIL::Module *mod, std::vector<RTLIL::Wire*> wires, std::vector<RTLIL::Cell*> cells,std::vector<RTLIL::SigSig> connections, std::string suffix, const Config *cfg){
        dict<RTLIL::SigSpec, RTLIL::SigSpec> wire_map;
        dict<RTLIL::Wire*, RTLIL::Wire*> output_map;
        dict<RTLIL::Cell *, RTLIL::Cell *> flip_flop_map;

        for(auto w : wires){
            // TODO: verify if this actually works, fix no clk/rst expansion
            if((cfg->preserve_module_ports && w->port_input) || (w->name == cfg->clock_port_name && !cfg->expand_clock) || (w->name == cfg->reset_port_name && !cfg->expand_reset)){
                wire_map[w] = w;
                continue;
            }

            // TODO: move attr to header
            if (is_tmr_error_out_wire(w)){
                continue;
            }

            RTLIL::Wire *w_b = mod->addWire(mod->uniquify(w->name.str() + suffix), w->width);
            w_b->port_input = w->port_input;
            w_b->port_output = w->port_output;
            w_b->start_offset = w->start_offset;
            w_b->upto = w->upto;
            w_b->attributes = w->attributes;

            wire_map[w] = w_b;

            if(w->port_output){
                output_map[w] = w_b;
            }

        }

        for (auto c :cells) {
          if (is_proper_submodule(c->module->design->module(c->type))) {
            continue;
          }

          RTLIL::Cell *c_b = mod->addCell(mod->uniquify(c->name.str() + suffix), c->type);

          // log("Looking at cell %u\n",
              // (is_flip_flop(c, worker, &known_ff_cell_names)));

          if (is_flip_flop(c, mod , cfg)) {
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

    void rename_wires_and_cells(RTLIL::Module *mod, std::vector<RTLIL::Wire*> wires, std::vector<RTLIL::Cell*> cells, std::string suffix, const Config *cfg){
        log_header(mod->design, "Renaming wires with suffix %s\n", suffix.c_str());
        for (auto w : wires) {
          if ((cfg->preserve_module_ports && (w->port_input)) ||
              (w->has_attribute(ID(tmrx_error_sink)))) {
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


    std::vector<RTLIL::Wire*> insert_output_voters(RTLIL::Module *mod, dict<RTLIL::Wire*, std::pair<RTLIL::Wire*, RTLIL::Wire*>> out_map, const Config *cfg){
        std::vector<RTLIL::Wire*> error_signals;
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
          mod->rename(
              res_wires.first,
              mod->uniquify(outputs.first->name.str().substr(
                  0, outputs.first->name.str().size() - (cfg->logic_path_1_suffix.size()))));
        }

        return error_signals;
    }

    std::vector<RTLIL::IdString> get_output_port_name(const RTLIL::Cell *cell, const RTLIL::Design *design) {
      std::vector<RTLIL::IdString> outputs = {};

      const RTLIL::Module *cell_mod = design->module(cell->type);
      if (cell_mod) {
          const_cast<RTLIL::Module*>(cell_mod)->fixup_ports();
        for (auto &conn : cell->connections()) {
          const RTLIL::Wire *wire = cell_mod->wire(conn.first);
          if (wire && wire->port_output) {
            outputs.push_back(conn.first);
          }
        }
      } else {
        for (auto &conn : cell->connections()) {
          if (cell->output(conn.first)) {
            outputs.push_back(conn.first);
          }
        }
      }

      return outputs;
    }

    std::vector<RTLIL::Wire*> insert_voter_after_ff(RTLIL::Module *mod, dict<Cell*, std::pair<Cell*, Cell*>> ff_map){
        std::vector<RTLIL::Wire*> error_signals;


        for (auto flip_flops : ff_map) {

          std::vector<RTLIL::IdString> output_ports = get_output_port_name(flip_flops.first, mod->design);

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
              std::pair<RTLIL::Wire *, RTLIL::Wire *> res_wires = insert_voter(mod, intermediate_wires, mod->design);
              mod->connect(res_wires.first, original_signals.at(i));

              error_signals.push_back(res_wires.second);
            }
          }
        }

        return error_signals;
    }



    std::vector<RTLIL::Wire*> connect_submodules_preserver_mod_ports(RTLIL::Module *mod, RTLIL::Cell* cell, dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> wire_map){

        std::vector<RTLIL::Wire*> error_signals;
        RTLIL::Design *design = mod->design;

        if(!is_proper_submodule(design->module(cell->type))){
            return error_signals;
        }

        auto [input_ports, output_ports] = get_port_names(cell, design);


        // TODO: Optimization do not insert voter if input into submodule is
        // input into worker module

        for (auto port : input_ports) {
          RTLIL::SigSpec sig_a = cell->getPort(port);
          RTLIL::SigSpec sig_b = wire_map.at(sig_a).first;
          RTLIL::SigSpec sig_c = wire_map.at(sig_a).second;
          std::vector<RTLIL::SigSpec> inputs = {sig_a, sig_b, sig_c};

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

    std::vector<RTLIL::Wire*> connect_submodules_mod_ports(RTLIL::Module *mod, RTLIL::Cell* cell, const Config *cell_cfg, dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> wire_map){

        std::vector<RTLIL::Wire*> error_signals;
        RTLIL::Design *design = mod->design;

        if(!is_proper_submodule(design->module(cell->type))){
            return error_signals;
        }

        // const RTLIL::Module *cell_mod = design->module(cell->type);

        dict<RTLIL::IdString, RTLIL::SigSpec> orig_connections;
        for (auto &conn : cell->connections()) {
            orig_connections[conn.first] = conn.second;
        }

        cell->connections_.clear();

        // TODO: handle  signal

        for (auto &orig_conn : orig_connections){
            RTLIL::IdString port = orig_conn.first;
            RTLIL::SigSpec sig = orig_conn.second;

            RTLIL::IdString port_a = RTLIL::IdString(port.str() + cell_cfg->logic_path_1_suffix);
            RTLIL::IdString port_b = RTLIL::IdString(port.str() + cell_cfg->logic_path_2_suffix);
            RTLIL::IdString port_c = RTLIL::IdString(port.str() + cell_cfg->logic_path_3_suffix);

          RTLIL::SigSpec sig_b = wire_map[sig].first;
          RTLIL::SigSpec sig_c = wire_map[sig].second;

          cell->setPort(port_a, sig);
          cell->setPort(port_b, sig_b);
          cell->setPort(port_c, sig_c);

        }


        return error_signals;
    }


    void logic_tmr_expansion(RTLIL::Module *mod, const ConfigManager *cfg_mgr){
        const Config *cfg = cfg_mgr->cfg(mod);
        std::vector<RTLIL::Wire*> error_signals = {};

        std::vector<RTLIL::Wire*> original_wires(mod->wires().begin(), mod->wires().end());
        std::vector<RTLIL::Cell*> original_cells(mod->cells().begin(), mod->cells().end());
        std::vector<RTLIL::SigSig> original_connections(mod->connections().begin(), mod->connections().end());
        std::vector<RTLIL::Wire*> error_wires;

        log_header(mod->design, "Logic TMR expansion");
        auto [wiremap_b, outputmap_b, flipflopmap_b] = insert_duplicate_logic(mod, original_wires, original_cells, original_connections, cfg->logic_path_2_suffix, cfg);
        auto [wiremap_c, outputmap_c, flipflopmap_c] = insert_duplicate_logic(mod, original_wires, original_cells, original_connections, cfg->logic_path_3_suffix, cfg);

        dict<RTLIL::Wire*, std::pair<RTLIL::Wire*, RTLIL::Wire*>> combined_output_map = zip_dicts(outputmap_b, outputmap_c);
        dict<RTLIL::SigSpec, std::pair<RTLIL::SigSpec, RTLIL::SigSpec>> combined_wire_map = zip_dicts(wiremap_b, wiremap_c);
        dict<RTLIL::Cell*, std::pair<RTLIL::Cell*, RTLIL::Cell*>> combined_ff_map = zip_dicts(flipflopmap_b, flipflopmap_c);

        for(auto cell : original_cells){
            RTLIL::Module *cell_mod = mod->design->module(cell->type);
            if(!is_proper_submodule(cell_mod)){
                continue;
            }
            const Config *cell_cfg = cfg_mgr->cfg(cell_mod);
            std::vector<RTLIL::Wire*> v_err_w;

            if(cell_cfg->preserve_module_ports){
                v_err_w = connect_submodules_preserver_mod_ports(mod, cell, combined_wire_map);
            } else {
                v_err_w = connect_submodules_mod_ports(mod, cell, cell_cfg, combined_wire_map);
            }
            error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
        }


        rename_wires_and_cells(mod, original_wires, original_cells, cfg->logic_path_1_suffix, cfg);



        if(cfg->preserve_module_ports){
            auto v_err_w = insert_output_voters(mod, combined_output_map, cfg);
            error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
        }

        // insert ff voters

        if(cfg->insert_voter_before_ff){
            log_error("Insert before ff not yet implemented");
        }

        if(cfg->insert_voter_after_ff){
            auto v_err_w = insert_voter_after_ff(mod, combined_ff_map);
            error_wires.insert(error_wires.end(), v_err_w.begin(), v_err_w.end());
        }


        connect_error_signal(mod, error_signals);
    }

    void full_module_tmr_expansion(RTLIL::Module *mod, const Config *cfg){
        std::vector<RTLIL::Wire*> error_wires;

        RTLIL::IdString org_mod_name = mod->name;
        std::string mod_name = mod->name.str() + "_tmrx_worker";

        mod->design->rename(mod, mod_name);
        RTLIL::Module *wrapper = mod->design->addModule(org_mod_name);
        dict<RTLIL::Wire*, std::vector<RTLIL::Wire*>> wire_map;



        std::vector<std::string> suffixes = (cfg->preserve_module_ports ? std::vector<std::string>{""} : std::vector<std::string>{cfg->logic_path_1_suffix, cfg->logic_path_2_suffix, cfg->logic_path_3_suffix});

        for(size_t i = 0; i < 3; i++){
            for (auto w : mod->wires()) {
                if(w->port_id == 0){
                    continue;
                }
                bool is_error_w = is_tmr_error_out_wire(w);

                if((cfg->preserve_module_ports || is_error_w)&& i > 0 ){
                    wire_map[w].push_back(wire_map.at(w).at(0));
                    continue;
                }

                string new_wire_name = (w->name.str() + (is_error_w ? "" : suffixes.at(i)));

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

		std::vector<RTLIL::Cell*> duplicates;
		std::vector<dict<RTLIL::Wire*, RTLIL::Wire*>> cell_ports;



		for(size_t i = 0; i < 3; i++){
	        RTLIL::Cell *cell = wrapper->addCell(NEW_ID, mod_name);
			duplicates.push_back(cell);

			cell_ports.push_back({});

			for(auto w :mod->wires()){
			    if(w->port_id == 0 ){
							continue;
				}
				RTLIL::Wire *w_con = wrapper->addWire(NEW_ID, w->width);
				cell_ports.at(i)[(w)] = w_con;
				cell->setPort(w->name, w_con);

				if(is_tmr_error_out_wire(w)){
                    error_wires.push_back(w_con);
				}

			}

		}


		if(cfg->tmr_mode_full_module_insert_voter_before_modules && !cfg->preserve_module_ports){
            for(auto wm : wire_map){
                if( !wm.first->port_input){
                        continue;
                }

                std::vector<RTLIL::Wire*> voter_outs;

                for (size_t i = 0; i < 3 ; i++) {
                    auto [v_out, err] = insert_voter(wrapper, {wm.second.at(0), wm.second.at(1), wm.second.at(2)}, wrapper->design);
                    error_wires.push_back(err);
                    voter_outs.push_back(v_out);
    			}

                wire_map[wm.first] = voter_outs;
    		}
		}

		if(cfg->tmr_mode_full_module_insert_voter_after_modules && !cfg->preserve_module_ports){
            for(auto wm : wire_map){
                if( !wm.first->port_output || is_tmr_error_out_wire(wm.first)){
                        continue;
                }

                std::vector<RTLIL::Wire*> voter_outs;

                for (size_t i = 0; i < 3 ; i++) {
                    auto [v_out, err] = insert_voter(wrapper, {cell_ports.at(0).at(wm.first), cell_ports.at(1).at(wm.first), cell_ports.at(2).at(wm.first)}, wrapper->design);
                    error_wires.push_back(err);
                    voter_outs.push_back(v_out);
    			}

                cell_ports.at(0).at(wm.first) = voter_outs.at(0);
                cell_ports.at(1).at(wm.first) = voter_outs.at(1);
                cell_ports.at(2).at(wm.first) = voter_outs.at(2);

    		}
		}



		if(cfg->preserve_module_ports){
            for(auto wm : wire_map){
                if(wm.first->port_output){

                    if(is_tmr_error_out_wire(wm.first)){
                        continue;
                    }

                    auto [v_out, err] = insert_voter(wrapper, {cell_ports.at(0).at(wm.first), cell_ports.at(1).at(wm.first), cell_ports.at(2).at(wm.first)}, wrapper->design);
                    error_wires.push_back(err);
                    cell_ports.at(0).at(wm.first) = v_out;
                    cell_ports.at(1).at(wm.first) = v_out;
                    cell_ports.at(2).at(wm.first) = v_out;
                }
            }
		}


		for (size_t i = 0; i < 3 ; i++) {
		    for(auto wm : wire_map){
				if(is_tmr_error_out_wire(wm.first)){
                    continue;
				}
                wrapper->connect(wm.second.at(i), cell_ports.at(i).at(wm.first));
			}
		}




		connect_error_signal(wrapper, error_wires);

    }

    // Todo: check allow for driven error signal
    void connect_error_signal(RTLIL::Module *mod, std::vector<RTLIL::Wire*> error_signals){
        log_header(mod->design, "Connecting Error Signals");

        RTLIL::Wire *sink = nullptr;
        for (auto w : mod->wires()) {
          if (is_tmr_error_out_wire(w)) {
            if (sink != nullptr) {
              log_error("Duplicate error sinks, only one allowed");
            }
            sink = w;
          }
        }
        if (sink != nullptr && !error_signals.empty()) {
          RTLIL::SigSpec last_wire = error_signals.back();
          error_signals.pop_back();
          for (auto s : error_signals) {
            last_wire = mod->Or(NEW_ID, last_wire, s);
          }
          mod->connect(sink, last_wire);
        }
    }

    void execute(std::vector<std::string> args, RTLIL::Design *design) override {
        log_header(design, "Executing TMRX pass (Triple Modular Redundancy).\n");
        log_push();

        // TODO: remove
        log("args passed: %i\n", args.size());
        for (auto argm : args) {
            log("%s\n", argm);
        }

        std::string config_file = "";

        for (size_t arg = 1; arg < args.size(); arg++) {
            if (args[arg] == "-c" && arg + 1 < args.size()) {
                config_file = args[++arg];
                continue;
            }
            break;
        }


        ConfigManager cfg_mgr(design, config_file);

        log("\n");

        log_header(design, "Sorting modules\n");
        TopoSort<RTLIL::IdString> modules_to_process;

        for (auto module : design->modules()) {
          if (!design->selected(module) || module->get_blackbox_attribute()) {
            continue;
          }

          modules_to_process.node(module->name);

          for (auto c : module->cells()) {
            if ( design->module(c->type) && (is_proper_submodule(design->module(c->type))) ) {
              modules_to_process.edge(module->name, c->type);
            }

            // if(design->module(c->type) == nullptr){
            //     log("Mod %s is null\n", c->type.c_str());
            // }
          }
        }


         modules_to_process.sort();

         for(auto it = modules_to_process.sorted.rbegin(); it != modules_to_process.sorted.rend(); ++it)  {
             RTLIL::IdString mod_name = *it;
             RTLIL::Module *worker = design->module(mod_name);
             if (!worker)
               continue;

             log("Processing Module %s\n", mod_name.c_str());
             log_pop();
             log_push();



             if(cfg_mgr.cfg(worker)->tmr_mode == Config::TmrMode::None){
                 continue;
             }

             if (cfg_mgr.cfg(worker)->tmr_mode == Config::TmrMode::LogicTMR) {
                  logic_tmr_expansion(worker, &cfg_mgr);
             }

             if (cfg_mgr.cfg(worker)->tmr_mode == Config::TmrMode::FullModuleTMR){
                 full_module_tmr_expansion(worker, cfg_mgr.cfg(worker));
             }





             worker->fixup_ports();
         }


        log_pop();
    }
} TmrxPass;

PRIVATE_NAMESPACE_END
