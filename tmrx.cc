#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct TmrxPass : public Pass {
  TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}

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

      // RTLIL::Module *tmp = new Module;
      // worker->cloneInto(tmp);

      // RTLIL::Wire *a = worker->addWire("\\a1", 1);
      // a->port_input = true;
      // a->port_id = 69;


      // RTLIL::Wire *y = worker->addWire("\\b1", 1);
      // y->port_output = true;
      // y->port_id = 420;

      // worker->addNeg(NEW_ID, a, y);
      // 
      for (auto w : worker->wires()){
          worker->rename(w,worker->uniquify(w->name.str()+"_a"));
      }

      worker->fixup_ports();

    }
  }
} TmrxPass;

PRIVATE_NAMESPACE_END
