// Fault injection testbench for fi_counter.
//
// Build in two modes via preprocessor:
//   -DTMR   — TMR model: inject bit-faults into the three register copies and
//             verify that majority voters mask them
//   (default) — plain model: sanity-check that the counter increments correctly
//
// Arguments:
//   --duration  N   number of counting cycles  (default: 50)
//   --num-faults N  max injection experiments   (default: 10)
//
// Fault injection is done without vrtlmod: Verilator 4.228 stores all
// internal signals directly in Vfi_counter___024root (accessible via
// dut.rootp->vlSymsp->TOP).  We flip bits of the three TMR register
// copies (fi_counter__DOT___09_ / _10_ / _11_) and the three voter-result
// copies (count_o_a / _b / _c) — one experiment per copy.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "verilated.h"
#include "Vfi_counter.h"

#if defined(TMR) || defined(PLAIN_FI)
#include "Vfi_counter___024root.h"
#include "Vfi_counter__Syms.h"
#endif

// One full clock cycle.
static void tick(Vfi_counter& dut) {
    dut.clk_i = 0; dut.eval();
    dut.clk_i = 1; dut.eval();
}

// Hold reset for `cycles` ticks, then release with en_i=1.
static void do_reset(Vfi_counter& dut, int cycles = 4) {
    dut.rst_ni = 0; dut.en_i = 0;
    for (int i = 0; i < cycles; i++) tick(dut);
    dut.rst_ni = 1; dut.en_i = 1;
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
    // Injectable targets: the three FF copies (_09_ / _10_ / _11_) and
    // the three voter-result wires (count_o_a / _b / _c).  Each is 8-bit.
    // We inject into copy _a or _b only — a single copy fault must be
    // masked by the 2-of-3 majority voter.
    // ----------------------------------------------------------------
    Vfi_counter dut;

    // Access the root struct that holds all internal signals.
    auto& root = dut.rootp->vlSymsp->TOP;

    struct Target {
        const char* name;
        CData*      ptr;   // pointer to the 8-bit register copy
    };

    // Inject into each FF copy and each voter-result wire independently.
    // All three FF copies and all three voter wires are targets; injecting
    // into a single copy at a time is always corrected by the 2-of-3 voter.
    std::vector<Target> targets = {
        { "_09_ (FF copy a)",  &root.fi_counter__DOT___09_ },
        { "_10_ (FF copy b)",  &root.fi_counter__DOT___10_ },
        { "_11_ (FF copy c)",  &root.fi_counter__DOT___11_ },
        { "count_o_a",         &root.fi_counter__DOT__count_o_a },
        { "count_o_b",         &root.fi_counter__DOT__count_o_b },
        { "count_o_c",         &root.fi_counter__DOT__count_o_c },
    };

    // Vary (target, bit, inject_at) — the three dimensions of the campaign.
    // Natural maximum: 6 targets × 8 bits × (duration-1) injection times.
    // num_faults caps this; experiments stop as soon as total reaches it.
    int failures = 0;
    int total    = 0;

outer:
    for (int at = 1; at < duration; at++) {
        for (auto& tgt : targets) {
            for (int bit = 0; bit < 8; bit++) {
                if (total >= num_faults) break;
                ++total;

                do_reset(dut);
                for (int c = 0; c < at; c++) tick(dut);

                // Inject: flip one bit in the target register copy.
                // The 2-of-3 majority voter corrects the output immediately.
                CData orig = *tgt.ptr;
                *tgt.ptr ^= static_cast<CData>(1u << bit);
                dut.eval();  // propagate injected value through combinational logic

                // Run remaining cycles (duration - at more ticks).
                for (int c = 0; c < duration - at; c++) tick(dut);

                // Expected count_o after `duration` enabled ticks = duration%256.
                auto expected = static_cast<uint8_t>(duration % 256);
                auto actual   = static_cast<uint8_t>(dut.count_o);
                if (actual != expected) {
                    std::cerr << "FAIL  target=" << tgt.name
                              << "  bit=" << bit << "  at=" << at
                              << "  expected=" << static_cast<int>(expected)
                              << "  got="      << static_cast<int>(actual)
                              << "  (orig=" << static_cast<int>(orig) << ")\n";
                    ++failures;
                }
            }
            if (total >= num_faults) break;
        }
        if (total >= num_faults) break;
    }

    int natural_max = (duration - 1) * static_cast<int>(targets.size()) * 8;
    std::cout << "Ran " << total << " fault injection experiment(s)"
              << " (natural max for this design: " << natural_max << ")\n";
    std::cout << "Result: " << (total - failures) << "/" << total
              << " faults masked\n";
    return failures ? 1 : 0;

#elif defined(PLAIN_FI)
    // ----------------------------------------------------------------
    // Plain model fault injection — same experiment as TMR mode but on
    // the unprotected counter.  A single-bit flip in count_q is never
    // corrected, so count_o will be wrong after every injection.
    //
    // Uses identical exit-code semantics to TMR mode: exits with the
    // number of faults that were NOT masked (count_o != expected).
    // For a plain counter that is every experiment, so this binary
    // always exits non-zero.  Meson declares the test should_fail: true
    // — it passes precisely when this binary exits != 0.
    // ----------------------------------------------------------------
    Vfi_counter dut;
    auto& root = dut.rootp->vlSymsp->TOP;

    struct Target { const char* name; CData* ptr; };
    std::vector<Target> targets = {
        { "count_q", &root.fi_counter__DOT__count_q },
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

                auto expected = static_cast<uint8_t>(duration % 256);
                auto actual   = static_cast<uint8_t>(dut.count_o);
                if (actual != expected) {
                    ++failures;  // fault leaked through — expected for plain counter
                } else {
                    // Fault made no difference — unexpectedly masked
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
    // without any injected faults.  This confirms that the testbench
    // setup (synthesis, Verilator elaboration, compilation) is correct
    // before trusting the fault-injection results.
    // ----------------------------------------------------------------
    Vfi_counter dut;
    do_reset(dut);
    for (int c = 0; c < duration; c++) tick(dut);

    auto expected = static_cast<uint8_t>(duration % 256);
    auto actual   = static_cast<uint8_t>(dut.count_o);
    if (actual != expected) {
        std::cerr << "FAIL TMR counter (no faults): expected=" << static_cast<int>(expected)
                  << "  got=" << static_cast<int>(actual) << "\n";
        return 1;
    }
    std::cout << "PASS TMR counter (no faults): count_o=" << static_cast<int>(actual)
              << " after " << duration << " cycles\n";
    return 0;

#else
    // ----------------------------------------------------------------
    // Plain model sanity check
    // ----------------------------------------------------------------
    Vfi_counter dut;
    do_reset(dut);
    for (int c = 0; c < duration; c++) tick(dut);

    auto expected = static_cast<uint8_t>(duration % 256);
    auto actual   = static_cast<uint8_t>(dut.count_o);
    if (actual != expected) {
        std::cerr << "FAIL plain counter: expected=" << static_cast<int>(expected)
                  << "  got=" << static_cast<int>(actual) << "\n";
        return 1;
    }
    std::cout << "PASS plain counter: count_o=" << static_cast<int>(actual)
              << " after " << duration << " cycles\n";
    return 0;
#endif
}
