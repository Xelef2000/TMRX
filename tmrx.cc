
#include "kernel/celltypes.h"
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/utils.h"
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

static dict<RTLIL::IdString, pool<std::string>> ff_sources;
// TODO: Verify if this is the easiest way
struct MarkFFPass : public Pass {
  MarkFFPass() : Pass("markff", "mark ff for later tmrx pass") {}

  void execute(std::vector<std::string>, RTLIL::Design *design) override {
    log_header(design, "Executing Mark Flip Flop Pass\n");
    log_push();

    for (auto module : design->modules()) {
      if (design->selected(module) && !module->get_blackbox_attribute()) {
        log("Scanning module %s for FFs\n", log_id(module));
        for (auto cell : module->cells()) {
          log("Looking at cell %u\n",
              (RTLIL::builtin_ff_cell_types().count(cell->type) > 0));

          if (RTLIL::builtin_ff_cell_types().count(cell->type) > 0) {
            if (cell->has_attribute(ID::src)) {
              std::string src = cell->get_string_attribute(ID::src);
              // cell->set_src_attribute(src.append("tmrx_ff"));
              // cell->set_string_attribute(ID("\\tmrx_ff_test"), "test");
              // cell->attributes[ID(\\tmrx_is_ff)] = RTLIL::Const(1);
              // cell->attributes[ID::keep] = RTLIL::Const(1);
              ff_sources[module->name].insert(src);
            }
          }
        }

        // mark submodules
        for (auto c : module->cells()) {
          if (design->module(c->type) != nullptr) {
            c->set_bool_attribute(ID(tmrx_is_proper_submodule), true);
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

  bool is_flip_flop(const RTLIL::Cell *cell, const RTLIL::Module *module,
                    const pool<RTLIL::IdString> *ff_cell_types) {
    if (RTLIL::builtin_ff_cell_types().count(cell->type) > 0) {
      return true;
    }

    log("Looking at cell atributes %s\n", cell->type.c_str());
    for (auto attr : cell->attributes) {
      log("Attr:%s Val:%s\n", attr.first.c_str(),
          attr.second.decode_string().c_str());
    }

    std::string src = cell->get_string_attribute(ID::src);
    if (ff_sources[module->name].count(src) > 0) {
      return true;
    }

    if (!ff_cell_types->empty() && ff_cell_types->count(cell->type)) {
      return true;
    }

    return false;
  }

  bool is_proper_submodule(RTLIL::Cell *cell) {
    if (cell->has_attribute(ID(tmrx_is_proper_submodule))) {
      return cell->get_bool_attribute(ID(tmrx_is_proper_submodule));
    }

    return false;
  }

  std::vector<RTLIL::IdString>
  get_output_port_name(const RTLIL::Cell *cell, const RTLIL::Design *design) {
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

  std::vector<RTLIL::IdString>
  get_input_port_name(const RTLIL::Cell *cell, const RTLIL::Design *design) {
    std::vector<RTLIL::IdString> outputs = {};

    const RTLIL::Module *cell_mod = design->module(cell->type);
    if (cell_mod) {

        const_cast<RTLIL::Module*>(cell_mod)->fixup_ports();
      for (auto &conn : cell->connections()) {
        const RTLIL::Wire *wire = cell_mod->wire(conn.first);
        if (wire && wire->port_input) {
          outputs.push_back(conn.first);
        }
      }
    } else {
      for (auto &conn : cell->connections()) {
        if (cell->input(conn.first)) {
          outputs.push_back(conn.first);
        }
      }
    }

    return outputs;
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

  void execute(vector<string>, Design *design) override {
    log_header(design, "Executing TMRX pass (Triple Modular Redundancy).\n");
    log_push();

    TopoSort<RTLIL::IdString> modules_to_process;

    for (auto module : design->modules()) {
      if (!design->selected(module) || module->get_blackbox_attribute()) {
        continue;
      }

      modules_to_process.node(module->name);

      for (auto c : module->cells()) {
        if (is_proper_submodule(c) && design->module(c->type)) {
          modules_to_process.edge(c->type, module->name);
        }
      }
    }

    modules_to_process.sort();

    for (auto mod_name : modules_to_process.sorted) {
      RTLIL::Module *worker = design->module(mod_name);
      if (!worker)
        continue;

      log("Transforming module %s\n", log_id(mod_name));

      log_pop();
      log_push();

      bool preserve_module_ports = false;
      size_t rename_sufix_length = 2;

      bool insert_voter_after_flip_flop = true;
      bool insert_voter_before_flip_flop = false;
      pool<RTLIL::IdString> known_ff_cell_names = {};
      std::vector<RTLIL::Wire *> error_signals = {};

      std::vector<RTLIL::Wire *> orinal_wires(worker->wires().begin(),
                                              worker->wires().end());
      std::vector<RTLIL::Cell *> original_cells(worker->cells().begin(),
                                                worker->cells().end());
      std::vector<RTLIL::SigSig> origina_connections(
          worker->connections().begin(), worker->connections().end());

      // Add B
      // Add wires
      dict<RTLIL::SigSpec, RTLIL::SigSpec> wire_map_b;
      dict<RTLIL::SigSpec, RTLIL::SigSpec> wire_map_c;
      dict<RTLIL::Wire *, std::vector<RTLIL::Wire *>> output_map;
      dict<RTLIL::Cell *, std::vector<RTLIL::Cell *>> flip_flop_map;
      log("Adding B wires\n");
      log_pop();
      log_push();
      for (auto w : orinal_wires) {
        if (preserve_module_ports && w->port_input) {
          wire_map_b[w] = w;
          continue;
        }

        if (w->has_attribute(ID(tmrx_error_sink))) {
          continue;
        }

        RTLIL::Wire *w_b =
            worker->addWire(worker->uniquify(w->name.str() + "_b"), w->width);
        w_b->port_input = w->port_input;
        w_b->port_output = w->port_output;
        w_b->start_offset = w->start_offset;
        w_b->upto = w->upto;
        w_b->attributes = w->attributes;

        wire_map_b[w] = w_b;

        // TODO: fix this
        if (w->port_output) {
          output_map[w] = {w, w_b};
        }
      }

      log("Adding B cells\n");
      log_pop();
      log_push();

      for (auto c : original_cells) {
        if (is_proper_submodule(c)) {
          continue;
        }

        RTLIL::Cell *c_b =
            worker->addCell(worker->uniquify(c->name.str() + "_b"), c->type);

        log("Looking at cell %u\n",
            (is_flip_flop(c, worker, &known_ff_cell_names)));

        if (is_flip_flop(c, worker, &known_ff_cell_names)) {
          // TODO: fix this
          flip_flop_map[c] = {c, c_b};
        }

        c_b->parameters = c->parameters;
        c_b->attributes = c->attributes;

        for (auto &connection : c->connections()) {
          RTLIL::SigSpec sig = connection.second;

          for (auto &it : wire_map_b) {
            sig.replace(it.first, it.second);
          }

          c_b->setPort(connection.first, sig);
        }
      }

      log("Adding B connections\n");
      log_pop();
      log_push();
      for (auto conn : origina_connections) {
        RTLIL::SigSpec first = conn.first;
        RTLIL::SigSpec second = conn.second;

        for (auto &w : wire_map_b) {
          first.replace(w.first, w.second);
          second.replace(w.first, w.second);
        }

        worker->connect(first, second);
      }

      log("Adding C Wires");
      log_pop();
      log_push();

      for (auto w : orinal_wires) {
        if (preserve_module_ports && w->port_input) {
          wire_map_c[w] = w;
          continue;
        }

        if (w->has_attribute(ID(tmrx_error_sink))) {
          continue;
        }

        RTLIL::Wire *w_c =
            worker->addWire(worker->uniquify(w->name.str() + "_c"), w->width);
        w_c->port_input = w->port_input;
        w_c->port_output = w->port_output;
        w_c->start_offset = w->start_offset;
        w_c->upto = w->upto;
        w_c->attributes = w->attributes;

        wire_map_c[w] = w_c;

        if (w->port_output) {
          output_map[w].push_back(w_c);
        }
      }

      log("Adding C Cells\n");
      log_pop();
      log_push();

      for (auto c : original_cells) {
        if (is_proper_submodule(c)) {
          continue;
        }
        RTLIL::Cell *c_c =
            worker->addCell(worker->uniquify(c->name.str() + "_c"), c->type);

        if ((is_flip_flop(c, worker, &known_ff_cell_names))) {
          flip_flop_map[c].push_back(c_c);
        }

        c_c->parameters = c->parameters;
        c_c->attributes = c->attributes;

        for (auto &connection : c->connections()) {
          RTLIL::SigSpec sig = connection.second;

          for (auto &it : wire_map_c) {
            sig.replace(it.first, it.second);
          }

          c_c->setPort(connection.first, sig);
        }
      }

      log("Adding C connections\n");
      log_pop();
      log_push();

      for (auto conn : origina_connections) {
        RTLIL::SigSpec first = conn.first;
        RTLIL::SigSpec second = conn.second;

        for (auto &w : wire_map_c) {
          first.replace(w.first, w.second);
          second.replace(w.first, w.second);
        }

        worker->connect(first, second);
      }

      // processing submodules

      if (preserve_module_ports) {
        for (auto c : original_cells) {
          if (is_proper_submodule(c)) {

            std::vector<RTLIL::IdString> output_ports =
                get_output_port_name(c, design);
            std::vector<RTLIL::IdString> input_ports =
                get_input_port_name(c, design);

            // TODO: Optimization do not insert voter if input into submodule is
            // input into worker module

            for (auto port : input_ports) {
              RTLIL::SigSpec sig_a = c->getPort(port);
              RTLIL::SigSpec sig_b = wire_map_b[sig_a];
              RTLIL::SigSpec sig_c = wire_map_c[sig_a];
              std::vector<RTLIL::SigSpec> inputs = {sig_a, sig_b, sig_c};

              std::pair<RTLIL::Wire *, RTLIL::Wire *> res =
                  insert_voter(worker, inputs, design);
              error_signals.push_back(res.second);
              c->setPort(port, res.first);
            }

            for (auto port : output_ports) {
              RTLIL::SigSpec sig_a = c->getPort(port);
              RTLIL::SigSpec sig_b = wire_map_b[sig_a];
              RTLIL::SigSpec sig_c = wire_map_c[sig_a];

              RTLIL::Wire *new_output = worker->addWire(NEW_ID, sig_a.size());
              c->setPort(port, new_output);
              worker->connect(new_output, sig_a);
              worker->connect(new_output, sig_b);
              worker->connect(new_output, sig_c);
            }
          }
        }
      } else {
        log("Connecting Submodules\n");
        for (auto c : original_cells) {
          if (is_proper_submodule(c)) {
              log("Connecting module %s\n", c->name.c_str());
              log("Cell type: %s\n", c->type.c_str());

              const RTLIL::Module *cell_mod = design->module(c->type);
              log("Found module definition: %d\n", cell_mod != nullptr);

              // if (cell_mod) {
              //     log("Module has %zu wires\n", cell_mod->wires().size());
              // }

              log("Cell has %zu connections\n", c->connections().size());

              dict<RTLIL::IdString, RTLIL::SigSpec> orig_connections;
              for (auto &conn : c->connections()) {
                  orig_connections[conn.first] = conn.second;
              }

              c->connections_.clear();

              for (auto &orig_conn : orig_connections){
                  RTLIL::IdString port = orig_conn.first;
                  RTLIL::SigSpec sig = orig_conn.second;

                  RTLIL::IdString port_a = RTLIL::IdString(port.str() + "_a");
                  RTLIL::IdString port_b = RTLIL::IdString(port.str() + "_b");
                  RTLIL::IdString port_c = RTLIL::IdString(port.str() + "_c");

              // TODO: check ig ports exist in sub module

                RTLIL::SigSpec sig_b = wire_map_b[sig];
                RTLIL::SigSpec sig_c = wire_map_c[sig];

                c->setPort(port_a, sig);
                c->setPort(port_b, sig_b);
                c->setPort(port_c, sig_c);

              }



          }
        }

      }

      log("Renaming wires\n");
      log_pop();
      log_push(); // Rename Wires, Cells
      for (auto w : orinal_wires) {
        if ((preserve_module_ports && (w->port_input)) ||
            (w->has_attribute(ID(tmrx_error_sink)))) {
          continue;
        }
        worker->rename(w, worker->uniquify(w->name.str() + "_a"));
      }

      log("Renaming Cells");
      log_pop();
      log_push();
      for (auto c : original_cells) {
        if (is_proper_submodule(c)) {
          continue;
        }

        worker->rename(c, worker->uniquify(c->name.str() + "_a"));
      }

      log("Inserting Voters\n");
      log_pop();
      log_push(); // insert voters
      // insert post flip flop voters
      if (insert_voter_after_flip_flop) {
        // TODO: voter insertion optimization, do not inser voter if preserve
        // ports and ff output geht directly to worker out

        for (auto flip_flops : flip_flop_map) {

          std::vector<RTLIL::IdString> output_ports =
              get_output_port_name(flip_flops.second.at(0), design);

          if (output_ports.empty()) {
            log("Cell Type: %s\n", flip_flops.second.at(0)->type.str().c_str());
            log_error("Flip Flop witout output found");
          }

          for (auto port : output_ports) {
            std::vector<RTLIL::SigSpec> intermediate_wires;
            std::vector<RTLIL::SigSpec> original_signals;

            for (auto ff : flip_flops.second) {
              RTLIL::SigSpec out_signal = ff->getPort(port);
              RTLIL::Wire *intermediate_wire =
                  worker->addWire(NEW_ID, out_signal.size());

              ff->setPort(port, intermediate_wire);
              intermediate_wires.push_back(intermediate_wire);
              original_signals.push_back(out_signal);
            }

            for (size_t i = 0; i < flip_flops.second.size(); i++) {
              std::pair<RTLIL::Wire *, RTLIL::Wire *> res_wires =
                  insert_voter(worker, intermediate_wires, design);
              worker->connect(res_wires.first, original_signals.at(i));

              error_signals.push_back(res_wires.second);
            }
          }
        }
      }

      // insert pre flip flop voters
      if (insert_voter_before_flip_flop) {
        log_error("ERROR: pre flip flop not yet implemented");
        abort();
      }

      // Insert output port voters
      std::vector<RTLIL::Wire *> wires(worker->wires().begin(),
                                       worker->wires().end());
      std::vector<RTLIL::Cell *> cells(worker->cells().begin(),
                                       worker->cells().end());
      if (preserve_module_ports) {
        for (auto outputs : output_map) {
          outputs.first->port_output = false;

          std::vector<RTLIL::SigSpec> out_sigs = {};

          for (auto output_wire : outputs.second) {
            output_wire->port_output = false;
            out_sigs.push_back(output_wire);
          }

          std::pair<RTLIL::Wire *, RTLIL::Wire *> res_wires =
              insert_voter(worker, out_sigs, design);
          error_signals.push_back(res_wires.second);

          res_wires.first->port_output = true;
          worker->rename(
              res_wires.first,
              worker->uniquify(outputs.first->name.str().substr(
                  0, outputs.first->name.str().size() - rename_sufix_length)));
        }
      }

      log("Connecting error wires\n");
      log_pop();
      log_push();
      // connect error signals
      RTLIL::Wire *sink = nullptr;
      for (auto w : orinal_wires) {
        if (w->has_attribute(ID(tmrx_error_sink))) {
          if (sink != nullptr) {
            log_error("Dublicate error sinks, only one allowed");
          }
          sink = w;
        }
      }
      if (sink != nullptr && !error_signals.empty()) {
        RTLIL::SigSpec last_wire = error_signals.back();
        error_signals.pop_back();
        for (auto s : error_signals) {
          last_wire = worker->Or(NEW_ID, last_wire, s);
        }
        worker->connect(sink, last_wire);
      }

      // TODO: clone bindings bindings_

      worker->fixup_ports();
    }

    log_pop();
  }
} TmrxPass;

PRIVATE_NAMESPACE_END
