#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include <vector>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN


struct TmrxPass : public Pass {
  TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}


  void dublicate_logic(RTLIL::Module &module, std::string suffix){

  }

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



      std::vector<RTLIL::Wire*> wires(worker->wires().begin(), worker->wires().end());

      std::vector<RTLIL::Cell*> cells(worker->cells().begin(), worker->cells().end());



      // Add B
      // Add wires
      dict<RTLIL::Wire*, RTLIL::Wire*> wire_map;

      for (auto w : wires) {
          RTLIL::Wire *w_b = worker->addWire(worker->uniquify(w->name.str() + "_b"), w->width);
          w_b->port_input  = w->port_input;
          w_b->port_output = w->port_output;
          w_b->start_offset = w->start_offset;
          w_b->upto = w->upto;

          wire_map[w] = w_b;

      }

      for (auto c : cells){
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


      // Rename Wires, Cells
      for (auto w : wires){
          worker->rename(w,worker->uniquify(w->name.str()+"_a"));
      }

      for(auto c : cells){
          worker->rename(c,worker->uniquify(c->name.str()+ "_a"));
      }



      worker->fixup_ports();

    }
  }
} TmrxPass;

PRIVATE_NAMESPACE_END
