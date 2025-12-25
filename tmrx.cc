#include "kernel/yosys.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct TmrxPass : public Pass {
	TmrxPass() : Pass("tmrx", "add triple modular redundancy") {}

	void execute(vector<string>, Design *design) override
	{
		log_header(design, "Executing TMRX pass (Triple Modular Redundancy).\n");


		std::vector<RTLIL::IdString> modules_to_process;
		for (auto module : design->modules()) {
			if (design->selected(module) && !module->get_blackbox_attribute()) {
				modules_to_process.push_back(module->name);
			}
		}

		for (auto mod_name : modules_to_process)
		{
			RTLIL::Module *worker = design->module(mod_name);
			if (!worker) continue;

			log("Transforming module %s\n", log_id(mod_name));

			std::string base = mod_name.str() + "_worker";
			RTLIL::IdString worker_name = base;
			int counter = 1;
			while (design->module(worker_name) != nullptr) {
				worker_name = base + "_" + std::to_string(counter++);
			}
			
			design->rename(worker, worker_name);

			RTLIL::Module *wrapper = design->addModule(mod_name);

			dict<RTLIL::IdString, RTLIL::Wire*> wrapper_ports;

			for (auto wire : worker->wires()) {
				if (wire->port_id == 0) continue;

				RTLIL::Wire *w = wrapper->addWire(wire->name, wire->width);
				w->port_id = wire->port_id;
				w->port_input = wire->port_input;
				w->port_output = wire->port_output;
				wrapper_ports[wire->name] = w;
			}
			wrapper->fixup_ports();

			std::vector<RTLIL::Cell*> instances;
			for (int i = 0; i < 3; i++) {
				RTLIL::Cell *cell = wrapper->addCell(NEW_ID, worker_name);
				instances.push_back(cell);

				for (auto wire : worker->wires()) {
					if (wire->port_input) {
						cell->setPort(wire->name, wrapper_ports[wire->name]);
					}
				}
			}

			for (auto wire : worker->wires()) {
				if (wire->port_output) {
					RTLIL::SigSpec inst_outs[3];

					for (int i = 0; i < 3; i++) {
						RTLIL::Wire *tmp = wrapper->addWire(NEW_ID, wire->width);
						instances[i]->setPort(wire->name, tmp);
						inst_outs[i] = tmp;
					}


					RTLIL::SigSpec tmp_and = wrapper->addWire(NEW_ID, wire->width);
					
					wrapper->addAnd(NEW_ID, inst_outs[0], inst_outs[1], tmp_and);
					
					wrapper->addAnd(NEW_ID, tmp_and, inst_outs[2], wrapper_ports[wire->name]);
				}
			}
		}
	}
} TmrxPass;

PRIVATE_NAMESPACE_END