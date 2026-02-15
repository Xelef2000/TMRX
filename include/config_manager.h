#ifndef TMRX_CONFIG_MANAGER_H
#define TMRX_CONFIG_MANAGER_H

#include "kernel/yosys.h"
#include "kernel/yosys_common.h"
#include <string>
#include "toml11/toml.hpp"

YOSYS_NAMESPACE_BEGIN

YOSYS_NAMESPACE_END

const std::string cfg_group_prefix = "group";
const std::string cfg_module_prefix = "module_";

struct Config{
    enum class TmrMode{
      None,
      FullModuleTMR,
      LogicTMR,
    };

    TmrMode tmr_mode;

    enum class TmrVoter{
        Default
    };

    TmrVoter tmr_voter;

    bool preserv_module_ports;

    bool insert_voter_before_ff;
    bool insert_voter_after_ff;

    bool tmr_mode_full_module_insert_voter_before_modules;
    bool tmr_mode_full_module_insert_voter_after_modules;

    std::string clock_port_name;
    bool expand_clock;

    std::string reset_port_name;
    bool expand_reset;

    Yosys::pool<Yosys::RTLIL::IdString> ff_cells;
    Yosys::pool<Yosys::RTLIL::IdString> additional_ff_cells;
    Yosys::pool<Yosys::RTLIL::IdString> excludet_ff_cells;

    std::string logic_path_1_suffix;
    std::string logic_path_2_suffix;
    std::string logic_path_3_suffix;

};

struct ConfigManager{
    private:
        void load_global_default_cfg();
        void load_default_groups_cfg();
        void validate_cfg();
        Config parse_config(const toml::value &t, const Config &default_cfg);
        Config parse_module_annotations(const Yosys::RTLIL::Module *mod, const Config &default_cfg);

        Config global_cfg;

        Yosys::dict<std::string, Config> group_cfg;
        Yosys::dict<Yosys::RTLIL::IdString, Config> cell_cfgs;



    public:
        ConfigManager(Yosys::RTLIL::Design *design, const std::string &config_file);
        const Config* cfg(Yosys::RTLIL::Module *mod);
        std::string cfg_as_string(Yosys::RTLIL::Module *mod);

};

#endif
