#ifndef TMRX_UTILS_H
#define TMRX_UTILS_H

#include "kernel/sigtools.h"
#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN

template <typename T, typename Q>
hashlib::dict<T, std::pair<Q, Q>> zip_dicts(const hashlib::dict<T, Q> &a,
                                            const hashlib::dict<T, Q> &b) {

    if (a.size() != b.size()) {
        log_error("zip_dicts: dicts have different sizes [Dict 1: %zu, Dict 2: %zu]\n", a.size(),
                  b.size());
    }

    hashlib::dict<T, std::pair<Q, Q>> result;

    result.reserve(a.size());

    for (const auto &[key, val_a] : a) {
        if (!b.count(key)) {
            log_error("zip_dicts: key in first dict is not in second\n");
        }
        result[key] = std::make_pair(val_a, b.at(key));
    }
    return result;
}

YOSYS_NAMESPACE_END

#endif
