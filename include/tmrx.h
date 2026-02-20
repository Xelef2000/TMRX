#ifndef TMRX_H
#define TMRX_H

#include "config_manager.h"
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

YOSYS_NAMESPACE_BEGIN

extern dict<RTLIL::IdString, pool<std::string>> ff_sources;

YOSYS_NAMESPACE_END

#endif
