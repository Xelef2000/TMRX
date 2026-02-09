#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN


struct TmrxPass : public Pass {
  TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}


  // void dublicate_logic(RTLIL::Module &module, std::string suffix){

  // }

  void execute(vector<string>, Design *design) override {
    log_header(design, "Executing TMRX pass (Triple Modular Redundancy).\n");

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

      bool preserve_module_ports = true;
      int rename_sufix_length = 2;

      std::vector<RTLIL::Wire*> orinal_wires(worker->wires().begin(), worker->wires().end());

      std::vector<RTLIL::Cell*> original_cells(worker->cells().begin(), worker->cells().end());



      // Add B
      // Add wires
      dict<RTLIL::Wire*, RTLIL::Wire*> wire_map;
      dict<RTLIL::Wire*, std::vector<RTLIL::Wire*>> output_map;

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

          // TODO: fix this
          if(w->port_output){
              output_map[w] = {w, w_b};
          }

      }

      for (auto c : original_cells){
          RTLIL::Cell *c_b = worker->addCell(worker->uniquify(c->name.str() + "_b"), c->type);

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

      wire_map.clear();

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

          if(w->port_output){
              output_map[w].push_back(w_c);
          }


      }

      for (auto c : original_cells){
          RTLIL::Cell *c_c = worker->addCell(worker->uniquify(c->name.str() + "_c"), c->type);

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

      std::vector<RTLIL::Wire*> wires(worker->wires().begin(), worker->wires().end());
      std::vector<RTLIL::Cell*> cells(worker->cells().begin(), worker->cells().end());
      if(preserve_module_ports){
          for(auto outputs : output_map){
              outputs.first->port_output = false;

              RTLIL::Wire* last_wire = outputs.first;

              for (auto output_wire : outputs.second){
                  output_wire->port_output = false;
                  // RTLIL::Wire* out = worker->addWire(NEW_ID, last_wire->width);

                  // worker->addAnd(NEW_ID, last_wire , output_wire, out);
                  // last_wire = out;
              }

              std::vector<RTLIL::Wire*> all_pairs = {};

              for (size_t i = 0; i < outputs.second.size(); i++){
                  for(size_t j = i+1; j < outputs.second.size(); j++){
                      RTLIL::Wire* and_res = worker->addWire(NEW_ID, outputs.first->width);
                      worker->addOr(NEW_ID, outputs.second.at(i), outputs.second.at(j), and_res);
                      all_pairs.push_back(and_res);
                  }
              }

              last_wire = all_pairs.at(0);
              for (auto it = all_pairs.begin()+1; it != all_pairs.end(); it++){
                  RTLIL::Wire* out = worker->addWire(NEW_ID, outputs.first->width);
                  worker->addAnd(NEW_ID,  last_wire, *it, out);
                  last_wire = out;
              }

              // RTLIL::Wire* output_a = outputs.first;
              // RTLIL::Wire* output_b = outputs.second.at(0);
              // RTLIL::Wire* output_c = outputs.second.at(1);

              // last_wire = worker->addWire(NEW_ID, output_a->width);
              // RTLIL::Wire* second_to_last = worker->addWire(NEW_ID,output_a->width);

              // worker->addAnd(NEW_ID, output_a, output_b, second_to_last);
              // worker->addAnd(NEW_ID, second_to_last, output_c,last_wire);

              last_wire->port_output = true;
              worker->rename(last_wire, worker->uniquify(outputs.first->name.str().substr(0, outputs.first->name.str().size() - rename_sufix_length)));
          }
      }

      // Connections?
      // bindings_

      worker->fixup_ports();

    }
  }
} TmrxPass;

PRIVATE_NAMESPACE_END
