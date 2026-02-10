#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN


struct TmrxPass : public Pass {
  TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}


  // void dublicate_logic(RTLIL::Module &module, std::string suffix){

  // }i
  //

  std::vector<RTLIL::IdString> get_output_port_name(const RTLIL::Cell *cell){
      std::vector<RTLIL::IdString> outputs = {};
      for (auto &conn : cell->connections()){
          if (cell->output(conn.first)){
              outputs.push_back(conn.first);
          }
      }

      return outputs;
  }

  RTLIL::Wire* insert_voter(RTLIL::Module* module, std::vector<RTLIL::Wire*> inputs){
      if(inputs.size() != 3){
          log_error("Voters are only intendt to be inserted with 3 inputs");
      }

      size_t wire_size = inputs.at(0)->width;
      std::vector<RTLIL::Wire*> all_pairs = {};

      for (size_t i = 0; i < inputs.size(); i++){
          for(size_t j = i+1; j < inputs.size(); j++){
              RTLIL::Wire* and_res = module->addWire(NEW_ID, wire_size);
              module->addOr(NEW_ID, inputs.at(i), inputs.at(j), and_res);
              all_pairs.push_back(and_res);
          }
      }

      RTLIL::Wire* last_wire = all_pairs.at(0);
      for (auto it = all_pairs.begin()+1; it != all_pairs.end(); it++){
          RTLIL::Wire* out = module->addWire(NEW_ID, wire_size);
          module->addAnd(NEW_ID,  last_wire, *it, out);
          last_wire = out;
      }

      return last_wire;
  }

  void execute(vector<string>, Design *design) override {
    log_header(design, "Executing TMRX pass (Triple Modular Redundancy).\n");
    log_push();

    std::vector<RTLIL::IdString> modules_to_process;
    for (auto module : design->modules()) {
      if (design->selected(module) && !module->get_blackbox_attribute()) {
        modules_to_process.push_back(module->name);
      }
    }

    for (auto mod_name : modules_to_process) {
      RTLIL::Module *worker = design->module(mod_name);
      if (!worker)
        continue;

      log("Transforming module %s\n", log_id(mod_name));

      bool preserve_module_ports = false;
      size_t rename_sufix_length = 2;

      bool insert_voter_after_flip_flop = true;
      bool insert_voter_before_flip_flop = false;

      std::vector<RTLIL::Wire*> orinal_wires(worker->wires().begin(), worker->wires().end());
      std::vector<RTLIL::Cell*> original_cells(worker->cells().begin(), worker->cells().end());
      std::vector<RTLIL::SigSig> origina_connections(worker->connections().begin(), worker->connections().end());



      // Add B
      // Add wires
      dict<RTLIL::Wire*, RTLIL::Wire*> wire_map;
      dict<RTLIL::SigSpec, RTLIL::Wire*> sigspec_map;
      dict<RTLIL::Wire*, std::vector<RTLIL::Wire*>> output_map;

      dict<RTLIL::Cell*, std::vector<RTLIL::Cell*>> flip_flop_map;




      for (auto w : orinal_wires) {
          if(preserve_module_ports && w->port_input){
              wire_map[w] = w;
              continue;
          }



          RTLIL::Wire *w_b = worker->addWire(worker->uniquify(w->name.str() + "_b"), w->width);
          w_b->port_input  = w->port_input;
          w_b->port_output = w->port_output;
          w_b->start_offset = w->start_offset;
          w_b->upto = w->upto;

          wire_map[w] = w_b;
          sigspec_map[w] = w_b;

          // TODO: fix this
          if(w->port_output){
              output_map[w] = {w, w_b};
          }

      }

      for (auto c : original_cells){
          RTLIL::Cell *c_b = worker->addCell(worker->uniquify(c->name.str() + "_b"), c->type);

          log("Looking at cell %u\n", (RTLIL::builtin_ff_cell_types().count(c->type) > 0));

          if (RTLIL::builtin_ff_cell_types().count(c->type) > 0){
              // TODO: fix this
              flip_flop_map[c] = {c, c_b};
          }

          c_b->parameters = c->parameters;
          c_b->attributes = c->attributes;

          for (auto &connection : c->connections()){
              RTLIL::SigSpec sig = connection.second;

              for (auto &it : wire_map){
                  sig.replace(it.first, it.second);
              }

              c_b->setPort(connection.first, sig);
          }

      }

      for (auto conn : origina_connections){
          RTLIL::Wire* first = sigspec_map[conn.first];
          RTLIL::Wire* second = sigspec_map[conn.second];

          worker->connect(first,second);
      }

      wire_map.clear();
      sigspec_map.clear();

      for (auto w : orinal_wires) {
          if(preserve_module_ports && w->port_input){
              wire_map[w] = w;
              continue;
          }


          RTLIL::Wire *w_c = worker->addWire(worker->uniquify(w->name.str() + "_c"), w->width);
          w_c->port_input  = w->port_input;
          w_c->port_output = w->port_output;
          w_c->start_offset = w->start_offset;
          w_c->upto = w->upto;

          wire_map[w] = w_c;
          sigspec_map[w] = w_c;

          if(w->port_output){
              output_map[w].push_back(w_c);
          }


      }

      for (auto c : original_cells){
          RTLIL::Cell *c_c = worker->addCell(worker->uniquify(c->name.str() + "_c"), c->type);


          if (RTLIL::builtin_ff_cell_types().count(c->type) > 0){
              flip_flop_map[c].push_back(c_c);
          }

          c_c->parameters = c->parameters;
          c_c->attributes = c->attributes;

          for (auto &connection : c->connections()){
              RTLIL::SigSpec sig = connection.second;

              for (auto &it : wire_map){
                  sig.replace(it.first, it.second);
              }

              c_c->setPort(connection.first, sig);
          }

      }


      for (auto conn : origina_connections){
          RTLIL::Wire* first = sigspec_map[conn.first];
          RTLIL::Wire* second = sigspec_map[conn.second];

          worker->connect(first,second);
      }



      // Rename Wires, Cells
      for (auto w : orinal_wires){
          if(preserve_module_ports && (w->port_input)){
              continue;
          }
          worker->rename(w,worker->uniquify(w->name.str()+"_a"));
      }

      for(auto c : original_cells){
          worker->rename(c,worker->uniquify(c->name.str()+ "_a"));
      }



      //insert voters
      //insert post flip flop voters
      if(insert_voter_after_flip_flop){

          for (auto flip_flops : flip_flop_map){
              std::vector<std::vector<std::pair<RTLIL::Wire*, RTLIL::SigSpec>>> all_new_out_coonections = {};
              size_t nr_outputs = 0;
              for (auto ff : flip_flops.second){
                std::vector<RTLIL::IdString> outputs = get_output_port_name(ff);
                if(outputs.size() == 0){
                    log_error("Flip Flop witout output found");
                }

                std::vector<std::pair<RTLIL::Wire*, RTLIL::SigSpec>> new_out_connections ={};
                nr_outputs = outputs.size();
                for (auto out : outputs){
                    RTLIL::SigSpec out_signal = ff->getPort(out);
                    RTLIL::Wire* intermediate_wire = worker->addWire(NEW_ID, out_signal.size());

                    ff->setPort(out, intermediate_wire);

                    // RTLIL::Cell *place_holder = worker->addNeg(NEW_ID, intermediate_wire, out_signal);

                    std::pair<RTLIL::Wire*, RTLIL::SigSpec> new_connection = {intermediate_wire, out_signal};
                    new_out_connections.push_back(new_connection);
                }

                all_new_out_coonections.push_back(new_out_connections);
              }

              std::vector<std::vector<std::pair<RTLIL::Wire*, RTLIL::SigSpec>>> reordered_new_out_coonections = {};
              for( size_t i = 0; i < nr_outputs; i++){
                  std::vector<std::pair<RTLIL::Wire*, RTLIL::SigSpec>> gate_out_connections = {};
                  for( auto co : all_new_out_coonections){
                      gate_out_connections.push_back(co.at(i));
                  }
                  reordered_new_out_coonections.push_back(gate_out_connections);
              }

              for (auto output_connections : reordered_new_out_coonections){
                  vector<RTLIL::Wire*> inputs;
                  vector<RTLIL::SigSpec> outputs;

                  for (auto c : output_connections){
                      inputs.push_back(c.first);
                      outputs.push_back(c.second);
                  }

                  for(auto output : outputs){
                      RTLIL::Wire* voter_out = insert_voter(worker, inputs);
                      worker->connect(voter_out, output);
                  }
              }


          }
      }

      //insert pre flip flop voters
      if(insert_voter_before_flip_flop){
          log_error("ERROR: pre flip flop not yet implemented");
          abort();
      }



      // Insert output port voters
      std::vector<RTLIL::Wire*> wires(worker->wires().begin(), worker->wires().end());
      std::vector<RTLIL::Cell*> cells(worker->cells().begin(), worker->cells().end());
      if(preserve_module_ports){
          for(auto outputs : output_map){
              outputs.first->port_output = false;


              for (auto output_wire : outputs.second){
                  output_wire->port_output = false;
              }

              RTLIL::Wire* last_wire = insert_voter(worker, outputs.second);


              last_wire->port_output = true;
              worker->rename(last_wire, worker->uniquify(outputs.first->name.str().substr(0, outputs.first->name.str().size() - rename_sufix_length)));
          }
      }


      // TODO: clone bindings bindings_

      worker->fixup_ports();

    }

    log_pop();
  }
} TmrxPass;

PRIVATE_NAMESPACE_END
