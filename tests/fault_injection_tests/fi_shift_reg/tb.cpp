// Fault injection testbench for fi_shift_reg.
//
// Build in four modes via preprocessor:
//   -DTMR        — TMR model: inject bit-faults into the three voter-result
//                  wires and verify that majority voters mask them
//   -DTMR_SANITY — TMR model: run without fault injection (testbench setup check)
//   -DPLAIN_FI   — plain model: inject into the single register copy (expected to fail)
//   (default)    — plain model: sanity-check that the shift register rotates correctly
//
// Arguments:
//   --duration  N   number of counting cycles  (default: 50)
//   --num-faults N  max injection experiments   (default: 10)
//
// Expected output after N enabled ticks from reset (initial value = 0x01):
//   data_o = 0x01 rotated left N positions = 1 << (N % 8)

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "verilated.h"
#include "Vfi_shift_reg.h"

#if defined(TMR) || defined(PLAIN_FI)
#include "Vfi_shift_reg___024root.h"
#include "Vfi_shift_reg__Syms.h"
#endif

static void tick(Vfi_shift_reg& dut) {
    dut.clk_i = 0; dut.eval();
    dut.clk_i = 1; dut.eval();
}

static void do_reset(Vfi_shift_reg& dut, int cycles = 4) {
    dut.rst_ni = 0; dut.en_i = 0;
    for (int i = 0; i < cycles; i++) tick(dut);
    dut.rst_ni = 1; dut.en_i = 1;
}

// Expected data_o after `n` enabled ticks from reset (initial shift_q = 0x01,
// rotates left by one each cycle).
static uint8_t expected_output(int n) {
    return static_cast<uint8_t>(1u << (n % 8));
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
    // Injectable targets: the three voter-result wires (data_o_a / _b / _c).
    // Each is 8-bit.  A single-copy fault must be masked by the 2-of-3
    // majority voter on the output.
    // ----------------------------------------------------------------
    Vfi_shift_reg dut;
    auto& root = dut.rootp->vlSymsp->TOP;

    struct Target {
        const char* name;
        CData*      ptr;
    };

    std::vector<Target> targets = {
        { "data_o_a", &root.fi_shift_reg__DOT__data_o_a },
        { "data_o_b", &root.fi_shift_reg__DOT__data_o_b },
        { "data_o_c", &root.fi_shift_reg__DOT__data_o_c },
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
                auto actual = static_cast<uint8_t>(dut.data_o);
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
    // Plain model fault injection — inject into the unprotected shift_q
    // register.  Any bit flip is never corrected, so data_o will be wrong
    // after every injection.
    // ----------------------------------------------------------------
    Vfi_shift_reg dut;
    auto& root = dut.rootp->vlSymsp->TOP;

    struct Target { const char* name; CData* ptr; };
    std::vector<Target> targets = {
        { "shift_q", &root.fi_shift_reg__DOT__shift_q },
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
                auto actual = static_cast<uint8_t>(dut.data_o);
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
    Vfi_shift_reg dut;
    do_reset(dut);
    for (int c = 0; c < duration; c++) tick(dut);

    auto exp    = expected_output(duration);
    auto actual = static_cast<uint8_t>(dut.data_o);
    if (actual != exp) {
        std::cerr << "FAIL TMR shift register (no faults): expected=" << static_cast<int>(exp)
                  << "  got=" << static_cast<int>(actual) << "\n";
        return 1;
    }
    std::cout << "PASS TMR shift register (no faults): data_o=" << static_cast<int>(actual)
              << " after " << duration << " cycles\n";
    return 0;

#else
    // ----------------------------------------------------------------
    // Plain model sanity check
    // ----------------------------------------------------------------
    Vfi_shift_reg dut;
    do_reset(dut);
    for (int c = 0; c < duration; c++) tick(dut);

    auto exp    = expected_output(duration);
    auto actual = static_cast<uint8_t>(dut.data_o);
    if (actual != exp) {
        std::cerr << "FAIL plain shift register: expected=" << static_cast<int>(exp)
                  << "  got=" << static_cast<int>(actual) << "\n";
        return 1;
    }
    std::cout << "PASS plain shift register: data_o=" << static_cast<int>(actual)
              << " after " << duration << " cycles\n";
    return 0;
#endif
}
