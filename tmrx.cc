
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

static dict<RTLIL::IdString, pool<std::string>> ff_sources;
// TODO: Verify if this is the easiest way
struct MarkFFPass : public Pass{
    MarkFFPass() : Pass("markff", "mark ff for later tmrx pass"){}

    void execute(std::vector<std::string>, RTLIL::Design *design) override {
        log_header(design, "Executing Mark Flip Flop Pass\n");
        log_push();

        for (auto module : design->modules()) {
          if (design->selected(module) && !module->get_blackbox_attribute()) {
              log("Scanning module %s for FFs\n", log_id(module));
              for (auto cell : module->cells()){
                log("Looking at cell %u\n", (RTLIL::builtin_ff_cell_types().count(cell->type) > 0));

                if (RTLIL::builtin_ff_cell_types().count(cell->type) > 0){
                      if (cell->has_attribute(ID::src)){
                          std::string src = cell->get_string_attribute(ID::src);
                          ff_sources[module->name].insert(src);
                      }
                }
              }
          }
        }
        log_pop();
    }
} MarkFFPass;


struct TmrxPass : public Pass {
  TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}


  // void dublicate_logic(RTLIL::Module &module, std::string suffix){

  // }i
  //

  bool is_flip_flop(const RTLIL::Cell *cell,const RTLIL::Module* module,const pool<RTLIL::IdString> *ff_cell_types, bool heurisitic_check = false) {
      if (RTLIL::builtin_ff_cell_types().count(cell->type) > 0){
          return true;
      }

      std::string src = cell->get_string_attribute(ID::src);
      if (ff_sources[module->name].count(src) > 0) {
          return true;
      }


      if(!ff_cell_types->empty() && ff_cell_types->count(cell->type)){
          return true;
      }

      if(heurisitic_check){

      }

      return false;
  }


  std::vector<RTLIL::IdString> get_output_port_name(const RTLIL::Cell *cell,const RTLIL::Design *design){
      std::vector<RTLIL::IdString> outputs = {};

      const RTLIL::Module *cell_mod = design->module(cell->type);
      if(cell_mod){
          for (auto &conn : cell->connections()){
              const RTLIL::Wire *wire = cell_mod->wire(conn.first);
              if(wire && wire->port_output){
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


  RTLIL::IdString createVoterCell(RTLIL::Design *design, size_t wire_width){

      RTLIL::IdString voter_name =  "\\tmrx_simple_voter_"+ std::to_string(wire_width);
      if(design->module(voter_name) != nullptr){
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

      RTLIL::Wire *out_y = voter->addWire("\\y",wire_width);
      out_y->port_output = true;

      RTLIL::SigSpec pair1 = voter->And(NEW_ID, in_a, in_b);
      RTLIL::SigSpec pair2 = voter->And(NEW_ID,in_a, in_c);
      RTLIL::SigSpec pair3 = voter->And(NEW_ID,in_b, in_c);

      RTLIL::SigSpec intermediate1 = voter->Or(NEW_ID, pair1, pair2);
      voter->addOr(NEW_ID, intermediate1, pair3, out_y);
      voter->fixup_ports();
      return voter_name;
  }


  RTLIL::Wire* insert_voter(RTLIL::Module* module, std::vector<RTLIL::Wire*> inputs, RTLIL::Design* design){
      if(inputs.size() != 3){
          log_error("Voters are only intendt to be inserted with 3 inputs");
      }

      size_t wire_width = inputs.at(0)->width;


      RTLIL::IdString voter_name = createVoterCell(design, wire_width);



      RTLIL::Wire* last_wire = module->addWire(NEW_ID,wire_width);
      RTLIL::Cell *voter_inst = module->addCell(NEW_ID, voter_name);

      voter_inst->setPort("\\a", inputs.at(0));
      voter_inst->setPort("\\b", inputs.at(1));
      voter_inst->setPort("\\c", inputs.at(2));
      voter_inst->setPort("\\y", last_wire);


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
      bool heurisitc_ff_detection = false;
      pool<RTLIL::IdString> known_ff_cell_names = {};

      std::vector<RTLIL::Wire*> orinal_wires(worker->wires().begin(), worker->wires().end());
      std::vector<RTLIL::Cell*> original_cells(worker->cells().begin(), worker->cells().end());
      std::vector<RTLIL::SigSig> origina_connections(worker->connections().begin(), worker->connections().end());



      // Add B
      // Add wires
      dict<RTLIL::Wire*, RTLIL::Wire*> wire_map;
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

          // TODO: fix this
          if(w->port_output){
              output_map[w] = {w, w_b};
          }

      }

      for (auto c : original_cells){
          RTLIL::Cell *c_b = worker->addCell(worker->uniquify(c->name.str() + "_b"), c->type);

          log("Looking at cell %u\n", (is_flip_flop(c, worker, &known_ff_cell_names, heurisitc_ff_detection)));


          if (is_flip_flop(c, worker, &known_ff_cell_names, heurisitc_ff_detection)){
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
          RTLIL::SigSpec first = conn.first;
          RTLIL::SigSpec second = conn.second;

          for (auto &w : wire_map){
              first.replace(w.first, w.second);
              second.replace(w.first, w.second);

          }

          worker->connect(first,second);
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


          if ((is_flip_flop(c, worker, &known_ff_cell_names, heurisitc_ff_detection))){
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
          RTLIL::SigSpec first = conn.first;
          RTLIL::SigSpec second = conn.second;

          for (auto &w : wire_map){
              first.replace(w.first, w.second);
              second.replace(w.first, w.second);

          }

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

              std::vector<RTLIL::IdString> output_ports = get_output_port_name(flip_flops.second.at(0), design);


              if(output_ports.empty()){
                  log("Cell Type: %s\n",flip_flops.second.at(0)->type.str().c_str() );
                  log_error("Flip Flop witout output found");
              }

              for (auto port : output_ports){
                  std::vector<RTLIL::Wire*> intermediate_wires;
                  std::vector<RTLIL::SigSpec> original_signals;

                  for(auto ff : flip_flops.second){
                      RTLIL::SigSpec out_signal = ff->getPort(port);
                      RTLIL::Wire* intermediate_wire = worker->addWire(NEW_ID, out_signal.size());

                      ff->setPort(port, intermediate_wire);
                      intermediate_wires.push_back(intermediate_wire);
                      original_signals.push_back(out_signal);
                  }

                  for (size_t i = 0; i < flip_flops.second.size();i++){
                      RTLIL::Wire* voter_out = insert_voter(worker, intermediate_wires, design);
                      worker->connect(voter_out, original_signals.at(i));
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

              RTLIL::Wire* last_wire = insert_voter(worker, outputs.second, design);


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
