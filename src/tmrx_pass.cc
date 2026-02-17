#include "config_manager.h"
#include "kernel/hashlib.h"
#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
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
      if ((mod != nullptr) && mod->has_attribute(ID(tmrx_is_proper_submodule))) {
        return mod->get_bool_attribute(ID(tmrx_is_proper_submodule));
      }

      return false;
    }


    bool is_flip_flop(const RTLIL::Cell *cell, const RTLIL::Module *module, const Config *cfg) {

      if(cfg->excludet_ff_cells.count(cell->type) != 0){
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
              outputs.push_back(conn.first);
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

    std::pair<RTLIL::Wire *, RTLIL::Wire *>
    insert_voter(RTLIL::Module *module, std::vector<RTLIL::SigSpec> inputs,
                 RTLIL::Design *design) {
      if (inputs.size() != 3) {
        log_error("Voters are only intendt to be inserted with 3 inputs");
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
            if((cfg->preserv_module_ports && w->port_input) || (w->name == cfg->clock_port_name && !cfg->expand_clock) || (w->name == cfg->reset_port_name && !cfg->expand_reset)){
                wire_map[w] = w;
                continue;
            }

            // TODO: move attr to header
            if (w->has_attribute(ID(tmrx_error_sink))){
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
          // if (is_proper_submodule(c->module->design->module(c->type))) {
          //   continue;
          // }

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
          // if ((cfg->preserv_module_ports && (w->port_input)) ||
          //     (w->has_attribute(ID(tmrx_error_sink)))) {
          //   continue;
          // }
          mod->rename(w, mod->uniquify(w->name.str() + suffix));
        }

        for (auto c : cells) {
          if (is_proper_submodule(c->module->design->module(c->type))) {
            continue;
          }

          mod->rename(c, mod->uniquify(c->name.str() + suffix));
        }

    }

    void logic_tmr_expansion(RTLIL::Module *mod, const Config *cfg){
        std::vector<RTLIL::Wire*> error_signals = {};

        std::vector<RTLIL::Wire*> original_wires(mod->wires().begin(), mod->wires().end());
        std::vector<RTLIL::Cell*> original_cells(mod->cells().begin(), mod->cells().end());
        std::vector<RTLIL::SigSig> original_connections(mod->connections().begin(), mod->connections().end());

        log_header(mod->design, "Logic TMR expansion");
        auto [wiremap_b, outputmapt_b, flipflopmap_b] = insert_duplicate_logic(mod, original_wires, original_cells, original_connections, cfg->logic_path_2_suffix, cfg);
        auto [wiremap_c, outputmapt_c, flipflopmap_c] = insert_duplicate_logic(mod, original_wires, original_cells, original_connections, cfg->logic_path_3_suffix, cfg);

        // connect submodules

        rename_wires_and_cells(mod, original_wires, original_cells, cfg->logic_path_1_suffix, cfg);



        if(cfg->preserv_module_ports){
            // add output voters
        }

        // insert ff voters

        // return error wires
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

             if(cfg_mgr.cfg(worker)->tmr_mode == Config::TmrMode::None){
                 continue;
             }

             if (cfg_mgr.cfg(worker)->tmr_mode == Config::TmrMode::LogicTMR) {
                 logic_tmr_expansion(worker, cfg_mgr.cfg(worker));

             }

             worker->fixup_ports();
         }


        log_pop();
    }
} TmrxPass;

PRIVATE_NAMESPACE_END
