// Fault injection testbench for fi_down_counter.
//
// Build in four modes via preprocessor:
//   -DTMR        — TMR model: inject bit-faults into the three voter-result
//                  wires and verify that majority voters mask them
//   -DTMR_SANITY — TMR model: run without fault injection (testbench setup check)
//   -DPLAIN_FI   — plain model: inject into the single register copy (expected to fail)
//   (default)    — plain model: sanity-check that the counter decrements correctly
//
// Arguments:
//   --duration  N   number of counting cycles  (default: 50)
//   --num-faults N  max injection experiments   (default: 10)
//
// Expected output after N enabled ticks from reset (initial count_q = 0xFF):
//   count_o = (uint8_t)(255 - N)

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "verilated.h"
#include "Vfi_down_counter.h"

#if defined(TMR) || defined(PLAIN_FI)
#include "Vfi_down_counter___024root.h"
#include "Vfi_down_counter__Syms.h"
#endif

static void tick(Vfi_down_counter& dut) {
    dut.clk_i = 0; dut.eval();
    dut.clk_i = 1; dut.eval();
}

static void do_reset(Vfi_down_counter& dut, int cycles = 4) {
    dut.rst_ni = 0; dut.en_i = 0;
    for (int i = 0; i < cycles; i++) tick(dut);
    dut.rst_ni = 1; dut.en_i = 1;
}

// Expected count_o after `n` enabled ticks from reset (initial count_q = 0xFF,
// decrements by one each cycle; wraps naturally in uint8).
static uint8_t expected_output(int n) {
    return static_cast<uint8_t>(255 - n);
}

int main(int argc, char* argv[]) {
    int duration   = 50;
    int num_faults = 10;

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--duration"   && i + 1 < argc) duration   = std::atoi(argv[++i]);
        if (arg == "--num-faults" && i + 1 < argc) num_faults = std::atoi(argv[++i]);
    }

#ifdef TMR
    // ----------------------------------------------------------------
    // TMR fault-injection mode
    //
    // Injectable targets: the three voter-result wires (count_o_a / _b / _c).
    // Each is 8-bit.  A single-copy fault must be masked by the 2-of-3
    // majority voter on the output.
    // ----------------------------------------------------------------
    Vfi_down_counter dut;
    auto& root = dut.rootp->vlSymsp->TOP;

    struct Target {
        const char* name;
        CData*      ptr;
    };

    std::vector<Target> targets = {
        { "count_o_a", &root.fi_down_counter__DOT__count_o_a },
        { "count_o_b", &root.fi_down_counter__DOT__count_o_b },
        { "count_o_c", &root.fi_down_counter__DOT__count_o_c },
    };

    int failures = 0;
    int total    = 0;

    for (int at = 1; at < duration && total < num_faults; at++) {
        for (auto& tgt : targets) {
            for (int bit = 0; bit < 8 && total < num_faults; bit++) {
                ++total;

                do_reset(dut);
                for (int c = 0; c < at; c++) tick(dut);

                CData orig = *tgt.ptr;
                *tgt.ptr ^= static_cast<CData>(1u << bit);
                dut.eval();

                for (int c = 0; c < duration - at; c++) tick(dut);

                auto exp    = expected_output(duration);
                auto actual = static_cast<uint8_t>(dut.count_o);
                if (actual != exp) {
                    std::cerr << "FAIL  target=" << tgt.name
                              << "  bit=" << bit << "  at=" << at
                              << "  expected=" << static_cast<int>(exp)
                              << "  got="      << static_cast<int>(actual)
                              << "  (orig=" << static_cast<int>(orig) << ")\n";
                    ++failures;
                }
            }
        }
    }

    int natural_max = (duration - 1) * static_cast<int>(targets.size()) * 8;
    std::cout << "Ran " << total << " fault injection experiment(s)"
              << " (natural max for this design: " << natural_max << ")\n";
    std::cout << "Result: " << (total - failures) << "/" << total
              << " faults masked\n";
    return failures ? 1 : 0;

#elif defined(PLAIN_FI)
    // ----------------------------------------------------------------
    // Plain model fault injection — inject into the unprotected count_q
    // register.  Any bit flip is never corrected, so count_o will be wrong
    // after every injection.
    // ----------------------------------------------------------------
    Vfi_down_counter dut;
    auto& root = dut.rootp->vlSymsp->TOP;

    struct Target { const char* name; CData* ptr; };
    std::vector<Target> targets = {
        { "count_q", &root.fi_down_counter__DOT__count_q },
    };

    int failures = 0;
    int total    = 0;

    for (int at = 1; at < duration && total < num_faults; at++) {
        for (auto& tgt : targets) {
            for (int bit = 0; bit < 8 && total < num_faults; bit++) {
                ++total;
                do_reset(dut);
                for (int c = 0; c < at; c++) tick(dut);

                *tgt.ptr ^= static_cast<CData>(1u << bit);
                dut.eval();

                for (int c = 0; c < duration - at; c++) tick(dut);

                auto exp    = expected_output(duration);
                auto actual = static_cast<uint8_t>(dut.count_o);
                if (actual != exp) {
                    ++failures;
                } else {
                    std::cerr << "UNEXPECTED MASK  target=" << tgt.name
                              << "  bit=" << bit << "  at=" << at << "\n";
                }
            }
        }
    }

    int natural_max = (duration - 1) * static_cast<int>(targets.size()) * 8;
    std::cout << "Ran " << total << " fault injection experiment(s)"
              << " (natural max for this design: " << natural_max << ")\n";
    std::cout << "Result: " << failures << "/" << total
              << " faults leaked (no TMR protection)\n";
    return failures ? 1 : 0;

#elif defined(TMR_SANITY)
    // ----------------------------------------------------------------
    // TMR model sanity check (no fault injection)
    //
    // Verifies that the TMR-ed design produces correct output when run
    // without any injected faults.
    // ----------------------------------------------------------------
    Vfi_down_counter dut;
    do_reset(dut);
    for (int c = 0; c < duration; c++) tick(dut);

    auto exp    = expected_output(duration);
    auto actual = static_cast<uint8_t>(dut.count_o);
    if (actual != exp) {
        std::cerr << "FAIL TMR down-counter (no faults): expected=" << static_cast<int>(exp)
                  << "  got=" << static_cast<int>(actual) << "\n";
        return 1;
    }
    std::cout << "PASS TMR down-counter (no faults): count_o=" << static_cast<int>(actual)
              << " after " << duration << " cycles\n";
    return 0;

#else
    // ----------------------------------------------------------------
    // Plain model sanity check
    // ----------------------------------------------------------------
    Vfi_down_counter dut;
    do_reset(dut);
    for (int c = 0; c < duration; c++) tick(dut);

    auto exp    = expected_output(duration);
    auto actual = static_cast<uint8_t>(dut.count_o);
    if (actual != exp) {
        std::cerr << "FAIL plain down-counter: expected=" << static_cast<int>(exp)
                  << "  got=" << static_cast<int>(actual) << "\n";
        return 1;
    }
    std::cout << "PASS plain down-counter: count_o=" << static_cast<int>(actual)
              << " after " << duration << " cycles\n";
    return 0;
#endif
}
