#ifndef TMRX_CONSTANTS_H
#define TMRX_CONSTANTS_H

#include "kernel/rtlil.h"
#include "kernel/yosys.h"

#include <cstddef>
#include <string>

YOSYS_NAMESPACE_BEGIN
namespace TMRX {

constexpr const char cfg_global_scope_name[] = "global";
constexpr const char cfg_group_scope_name[] = "group";
constexpr const char cfg_module_scope_name[] = "module";
constexpr const char cfg_specific_module_scope_name[] = "specific_module";
constexpr const char cfg_logic_scope_name[] = "logic";
constexpr const char cfg_full_module_scope_name[] = "full_module";
constexpr const char cfg_groups_key_name[] = "groups";

constexpr const char cfg_tmr_mode_key_name[] = "tmr_mode";
constexpr const char cfg_tmr_voter_key_name[] = "tmr_voter";
constexpr const char cfg_tmr_voter_file_key_name[] = "tmr_voter_file";
constexpr const char cfg_tmr_voter_module_key_name[] = "tmr_voter_module";
constexpr const char cfg_tmr_voter_clock_port_name_key_name[] = "tmr_voter_clock_port_name";
constexpr const char cfg_tmr_voter_reset_port_name_key_name[] = "tmr_voter_reset_port_name";
constexpr const char cfg_tmr_voter_clock_net_key_name[] = "tmr_voter_clock_net";
constexpr const char cfg_tmr_voter_reset_net_key_name[] = "tmr_voter_reset_net";
constexpr const char cfg_tmr_voter_safe_mode_key_name[] = "tmr_voter_safe_mode";
constexpr const char cfg_preserve_module_ports_key_name[] = "preserve_module_ports";
constexpr const char cfg_clock_port_names_key_name[] = "clock_port_names";
constexpr const char cfg_expand_clock_key_name[] = "expand_clock";
constexpr const char cfg_reset_port_names_key_name[] = "reset_port_names";
constexpr const char cfg_expand_reset_key_name[] = "expand_reset";
constexpr const char cfg_error_port_name_key_name[] = "error_port_name";
constexpr const char cfg_auto_error_port_key_name[] = "auto_error_port";
constexpr const char cfg_insert_voter_before_ff_key_name[] = "insert_voter_before_ff";
constexpr const char cfg_insert_voter_after_ff_key_name[] = "insert_voter_after_ff";
constexpr const char cfg_ff_cells_key_name[] = "ff_cells";
constexpr const char cfg_additional_ff_cells_key_name[] = "additional_ff_cells";
constexpr const char cfg_excluded_ff_cells_key_name[] = "excluded_ff_cells";
constexpr const char cfg_logic_path_1_suffix_key_name[] = "logic_path_1_suffix";
constexpr const char cfg_logic_path_2_suffix_key_name[] = "logic_path_2_suffix";
constexpr const char cfg_logic_path_3_suffix_key_name[] = "logic_path_3_suffix";
constexpr const char cfg_insert_voter_before_modules_key_name[] = "insert_voter_before_modules";
constexpr const char cfg_insert_voter_after_modules_key_name[] = "insert_voter_after_modules";
constexpr const char cfg_insert_voter_on_clock_nets_key_name[] = "insert_voter_on_clock_nets";
constexpr const char cfg_insert_voter_on_reset_nets_key_name[] = "insert_voter_on_reset_nets";

constexpr const char cfg_group_assignment_attr_name[] = "\\tmrx_assign_to_group";
constexpr const char cfg_tmr_mode_attr_name[] = "\\tmrx_tmr_mode";
constexpr const char cfg_tmr_voter_attr_name[] = "\\tmrx_tmr_voter";
constexpr const char cfg_tmr_voter_safe_mode_attr_name[] = "\\tmrx_tmr_voter_safe_mode";
constexpr const char cfg_tmr_mode_full_module_insert_voter_before_modules_attr_name[] =
    "\\tmrx_tmr_mode_full_module_insert_voter_before_modules";
constexpr const char cfg_tmr_mode_full_module_insert_voter_after_modules_attr_name[] =
    "\\tmrx_tmr_mode_full_module_insert_voter_after_modules";
constexpr const char cfg_tmr_voter_file_attr_name[] = "\\tmrx_tmr_voter_file";
constexpr const char cfg_tmr_voter_module_attr_name[] = "\\tmrx_tmr_voter_module";
constexpr const char cfg_tmr_voter_clock_port_name_attr_name[] = "\\tmrx_tmr_voter_clock_port_name";
constexpr const char cfg_tmr_voter_reset_port_name_attr_name[] = "\\tmrx_tmr_voter_reset_port_name";
constexpr const char cfg_tmr_voter_clock_net_attr_name[] = "\\tmrx_tmr_voter_clock_net";
constexpr const char cfg_tmr_voter_reset_net_attr_name[] = "\\tmrx_tmr_voter_reset_net";
constexpr const char cfg_tmr_mode_full_module_insert_voter_on_clock_nets_attr_name[] =
    "\\tmrx_tmr_mode_full_module_insert_voter_on_clock_nets";
constexpr const char cfg_tmr_mode_full_module_insert_voter_on_reset_nets_attr_name[] =
    "\\tmrx_tmr_mode_full_module_insert_voter_on_reset_nets";
constexpr const char cfg_tmr_preserve_module_ports_attr_name[] = "\\tmrx_tmr_preserve_module_ports";
constexpr const char cfg_insert_voter_before_ff_attr_name[] = "\\tmrx_insert_voter_before_ff";
constexpr const char cfg_insert_voter_after_ff_attr_name[] = "\\tmrx_insert_voter_after_ff";
constexpr const char cfg_clock_port_name_attr_name[] = "\\tmrx_clock_port_name";
constexpr const char cfg_rst_port_name_attr_name[] = "\\tmrx_rst_port_name";
constexpr const char cfg_expand_clock_attr_name[] = "\\tmrx_expand_clock";
constexpr const char cfg_expand_rst_attr_name[] = "\\tmrx_expand_rst";
constexpr const char cfg_logic_path_1_suffix_attr_name[] = "\\tmrx_logic_path_1_suffix";
constexpr const char cfg_logic_path_2_suffix_attr_name[] = "\\tmrx_logic_path_2_suffix";
constexpr const char cfg_logic_path_3_suffix_attr_name[] = "\\tmrx_logic_path_3_suffix";
constexpr const char cfg_error_port_name_attr_name[] = "\\tmrx_error_port_name";
constexpr const char cfg_auto_error_port_attr_name[] = "\\tmrx_auto_error_port";

constexpr const char cfg_tmr_mode_none_name[] = "None";
constexpr const char cfg_tmr_mode_full_module_tmr_name[] = "FullModuleTMR";
constexpr const char cfg_tmr_mode_logic_tmr_name[] = "LogicTMR";
constexpr const char cfg_tmr_voter_default_name[] = "Default";
constexpr const char cfg_tmr_voter_custom_name[] = "Custom";
constexpr const char cfg_unknown_name[] = "Unknown";

constexpr const char cfg_true_value[] = "1";
constexpr char cfg_attr_list_separator = ';';
constexpr char cfg_specific_module_separator = '$';
constexpr const char cfg_root_context_name[] = "<root>";

constexpr const char cfg_default_clock_port_name[] = "\\clk_i";
constexpr const char cfg_default_reset_port_name[] = "\\rst_ni";
constexpr const char cfg_default_logic_path_1_suffix[] = "_a";
constexpr const char cfg_default_logic_path_2_suffix[] = "_b";
constexpr const char cfg_default_logic_path_3_suffix[] = "_c";
constexpr const char cfg_default_black_box_module_group_name[] = "black_box_module";
constexpr const char cfg_default_cdc_module_group_name[] = "cdc_module";

constexpr std::size_t tmrx_replication_factor = 3;
constexpr const char tmrx_impl_module_suffix[] = "_tmrx_impl";
constexpr const char tmrx_worker_module_suffix[] = "_tmrx_worker";
constexpr const char tmrx_signal_name_const[] = "const";
constexpr const char tmrx_signal_name_separator[] = "_";
constexpr const char tmrx_voter_module_prefix[] = "\\tmrx_voter_";
constexpr const char tmrx_voter_width_separator[] = "_w";
constexpr const char tmrx_auto_error_port_name[] = "\\tmrx_err_o";

constexpr const char tmrx_voter_port_a_name[] = "a";
constexpr const char tmrx_voter_port_b_name[] = "b";
constexpr const char tmrx_voter_port_c_name[] = "c";
constexpr const char tmrx_voter_port_y_name[] = "y";
constexpr const char tmrx_voter_port_err_name[] = "err";

constexpr const char tmrx_voter_port_a_id[] = "\\a";
constexpr const char tmrx_voter_port_b_id[] = "\\b";
constexpr const char tmrx_voter_port_c_id[] = "\\c";
constexpr const char tmrx_voter_port_y_id[] = "\\y";
constexpr const char tmrx_voter_port_err_id[] = "\\err";

static const char *const tmrx_voter_input_labels[tmrx_replication_factor] = {
    tmrx_voter_port_a_name,
    tmrx_voter_port_b_name,
    tmrx_voter_port_c_name,
};

inline std::string escapeRtlilId(const std::string &name) { return std::string("\\") + name; }

inline Yosys::RTLIL::IdString makeRtlilId(const std::string &name) {
    return Yosys::RTLIL::IdString(escapeRtlilId(name));
}

const auto ATTRIBUTE_IS_PROPER_SUBMODULE = ID(tmrx_is_proper_submodule);
const auto ATTRIBUTE_IMPL_MODULE = ID(tmrx_impl_module);
const auto ATTRIBUTE_CLK_PORT = ID(tmrx_clk_port);
const auto ATTRIBUTE_RST_PORT = ID(tmrx_rst_port);
const auto ATTRIBUTE_ERROR_SINK = ID(tmrx_error_sink);

} // namespace TMRX
YOSYS_NAMESPACE_END

#endif
