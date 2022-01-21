#include "AvailableKernels.hpp"
#include "Gates.hpp"
#include "TestHelpers.hpp"
#include "Util.hpp"

#include <catch2/catch.hpp>

#include <algorithm>
#include <complex>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * @file This file contains tests for non-parameterized gates. List of such
 * gates are [PauliX, PauliY, PauliZ, Hadamard, S, T, CNOT, SWAP, CZ, Toffoli,
 * CSWAP].
 */
using namespace Pennylane;

namespace {
using std::vector;
}

/**
 * @brief Run test suit only when the gate is defined
 */
#define PENNYLANE_RUN_TEST(GATE_NAME)                                          \
    template <typename PrecisionT, class GateImplementation,                   \
              typename U = void>                                               \
    struct TestApply##GATE_NAME##IfDefined {                                   \
        static void run() {}                                                   \
    };                                                                         \
    template <typename PrecisionT, class GateImplementation>                   \
    struct TestApply##GATE_NAME##IfDefined<                                    \
        PrecisionT, GateImplementation,                                        \
        std::enable_if_t<std::is_pointer_v<decltype(                           \
            &GateImplementation::template apply##GATE_NAME<PrecisionT>)>>> {   \
        static void run() {                                                    \
            testApply##GATE_NAME<PrecisionT, GateImplementation>();            \
        }                                                                      \
    };                                                                         \
    template <typename PrecisionT, typename TypeList>                          \
    struct TestApply##GATE_NAME##ForKernels {                                  \
        static void run() {                                                    \
            TestApply##GATE_NAME##IfDefined<PrecisionT,                        \
                                            typename TypeList::Type>::run();   \
            TestApply##GATE_NAME##ForKernels<PrecisionT,                       \
                                             typename TypeList::Next>::run();  \
        }                                                                      \
    };                                                                         \
    template <typename PrecisionT>                                             \
    struct TestApply##GATE_NAME##ForKernels<PrecisionT, void> {                \
        static void run() {}                                                   \
    };                                                                         \
    TEMPLATE_TEST_CASE("GateImplementation::apply" #GATE_NAME,                 \
                       "[GateImplementations_Nonparam]", float,                \
                       double) {                                               \
        using PrecisionT = TestType;                                           \
        TestApply##GATE_NAME##ForKernels<PrecisionT, AvailableKernels>::run(); \
    }

/*******************************************************************************
 * Single-qubit gates
 ******************************************************************************/
template <typename PrecisionT, class GateImplementation>
void testApplyPauliX() {
    const size_t num_qubits = 3;
    for (size_t index = 0; index < num_qubits; index++) {
        auto st = create_zero_state<PrecisionT>(num_qubits);
        CHECK(st[0] == Util::ONE<PrecisionT>());

        GateImplementation::applyPauliX(st.data(), num_qubits, {index}, false);
        CHECK(st[0] == Util::ZERO<PrecisionT>());
        CHECK(st[0b1 << (num_qubits - index - 1)] == Util::ONE<PrecisionT>());
    }
}
PENNYLANE_RUN_TEST(PauliX)

template <typename PrecisionT, class GateImplementation>
void testApplyPauliY() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;

    constexpr ComplexPrecisionT p =
        Util::ConstMult(static_cast<PrecisionT>(0.5),
                        Util::ConstMult(Util::INVSQRT2<PrecisionT>(),
                                        Util::IMAG<PrecisionT>()));
    constexpr ComplexPrecisionT m = Util::ConstMult(-1, p);

    const std::vector<std::vector<ComplexPrecisionT>> expected_results = {
        {m, m, m, m, p, p, p, p},
        {m, m, p, p, m, m, p, p},
        {m, p, m, p, m, p, m, p}};

    for (size_t index = 0; index < num_qubits; index++) {
        auto st = create_plus_state<PrecisionT>(num_qubits);

        GateImplementation::applyPauliY(st.data(), num_qubits, {index}, false);

        CHECK(isApproxEqual(st, expected_results[index]));
    }
}
PENNYLANE_RUN_TEST(PauliY)

template <typename PrecisionT, class GateImplementation>
void testApplyPauliZ() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;

    constexpr ComplexPrecisionT p(static_cast<PrecisionT>(0.5) *
                                  Util::INVSQRT2<PrecisionT>());
    constexpr ComplexPrecisionT m(Util::ConstMult(-1, p));

    const std::vector<std::vector<ComplexPrecisionT>> expected_results = {
        {p, p, p, p, m, m, m, m},
        {p, p, m, m, p, p, m, m},
        {p, m, p, m, p, m, p, m}};

    for (size_t index = 0; index < num_qubits; index++) {
        auto st = create_plus_state<PrecisionT>(num_qubits);
        GateImplementation::applyPauliZ(st.data(), num_qubits, {index}, false);

        CHECK(isApproxEqual(st, expected_results[index]));
    }
}
PENNYLANE_RUN_TEST(PauliZ)

template <typename PrecisionT, class GateImplementation>
void testApplyHadamard() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;
    for (size_t index = 0; index < num_qubits; index++) {
        auto st = create_zero_state<PrecisionT>(num_qubits);

        CHECK(st[0] == ComplexPrecisionT{1, 0});
        GateImplementation::applyHadamard(st.data(), num_qubits, {index},
                                          false);

        ComplexPrecisionT expected(1 / std::sqrt(2), 0);
        CHECK(expected.real() == Approx(st[0].real()));
        CHECK(expected.imag() == Approx(st[0].imag()));

        CHECK(expected.real() ==
              Approx(st[0b1 << (num_qubits - index - 1)].real()));
        CHECK(expected.imag() ==
              Approx(st[0b1 << (num_qubits - index - 1)].imag()));
    }
}
PENNYLANE_RUN_TEST(Hadamard)

template <typename PrecisionT, class GateImplementation> void testApplyS() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;

    constexpr ComplexPrecisionT r(static_cast<PrecisionT>(0.5) *
                                  Util::INVSQRT2<PrecisionT>());
    constexpr ComplexPrecisionT i(Util::ConstMult(r, Util::IMAG<PrecisionT>()));

    const std::vector<std::vector<ComplexPrecisionT>> expected_results = {
        {r, r, r, r, i, i, i, i},
        {r, r, i, i, r, r, i, i},
        {r, i, r, i, r, i, r, i}};

    for (size_t index = 0; index < num_qubits; index++) {
        auto st = create_plus_state<PrecisionT>(num_qubits);

        GateImplementation::applyS(st.data(), num_qubits, {index}, false);

        CHECK(isApproxEqual(st, expected_results[index]));
    }
}
PENNYLANE_RUN_TEST(S)

template <typename PrecisionT, class GateImplementation> void testApplyT() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;
    // Test using |+++> state

    ComplexPrecisionT r(1.0 / (2.0 * std::sqrt(2)), 0);
    ComplexPrecisionT i(1.0 / 4, 1.0 / 4);

    const std::vector<std::vector<ComplexPrecisionT>> expected_results = {
        {r, r, r, r, i, i, i, i},
        {r, r, i, i, r, r, i, i},
        {r, i, r, i, r, i, r, i}};

    for (size_t index = 0; index < num_qubits; index++) {
        auto st = create_plus_state<PrecisionT>(num_qubits);

        GateImplementation::applyT(st.data(), num_qubits, {index}, false);

        CHECK(isApproxEqual(st, expected_results[index]));
    }
}
PENNYLANE_RUN_TEST(T)
/*******************************************************************************
 * Two-qubit gates
 ******************************************************************************/

template <typename PrecisionT, class GateImplementation> void testApplyCNOT() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;
    auto st = create_zero_state<PrecisionT>(num_qubits);

    // Test using |+00> state to generate 3-qubit GHZ state
    GateImplementation::applyHadamard(st.data(), num_qubits, {0}, false);

    for (size_t index = 1; index < num_qubits; index++) {
        GateImplementation::applyCNOT(st.data(), num_qubits, {index - 1, index},
                                      false);
    }
    CHECK(st.front() == Util::INVSQRT2<PrecisionT>());
    CHECK(st.back() == Util::INVSQRT2<PrecisionT>());
}
PENNYLANE_RUN_TEST(CNOT)

// NOLINTNEXTLINE: Avoiding complexity errors
template <typename PrecisionT, class GateImplementation> void testApplySWAP() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;
    auto ini_st = create_zero_state<PrecisionT>(num_qubits);

    // Test using |+10> state
    GateImplementation::applyHadamard(ini_st.data(), num_qubits, {0}, false);
    GateImplementation::applyPauliX(ini_st.data(), num_qubits, {1}, false);

    CHECK(ini_st == std::vector<ComplexPrecisionT>{
                        Util::ZERO<PrecisionT>(), Util::ZERO<PrecisionT>(),
                        Util::INVSQRT2<PrecisionT>(), Util::ZERO<PrecisionT>(),
                        Util::ZERO<PrecisionT>(), Util::ZERO<PrecisionT>(),
                        Util::INVSQRT2<PrecisionT>(),
                        Util::ZERO<PrecisionT>()});

    SECTION("SWAP0,1 |+10> -> |1+0>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>()};
        auto sv01 = ini_st;
        auto sv10 = ini_st;

        GateImplementation::applySWAP(sv01.data(), num_qubits, {0, 1}, false);
        GateImplementation::applySWAP(sv10.data(), num_qubits, {1, 0}, false);

        CHECK(sv01 == expected);
        CHECK(sv10 == expected);
    }

    SECTION("SWAP0,2 |+10> -> |01+>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>()};

        auto sv02 = ini_st;
        auto sv20 = ini_st;

        GateImplementation::applySWAP(sv02.data(), num_qubits, {0, 2}, false);
        GateImplementation::applySWAP(sv20.data(), num_qubits, {2, 0}, false);

        CHECK(sv02 == expected);
        CHECK(sv20 == expected);
    }
    SECTION("SWAP1,2 |+10> -> |+01>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>()};

        auto sv12 = ini_st;
        auto sv21 = ini_st;

        GateImplementation::applySWAP(sv12.data(), num_qubits, {1, 2}, false);
        GateImplementation::applySWAP(sv21.data(), num_qubits, {2, 1}, false);

        CHECK(sv12 == expected);
        CHECK(sv21 == expected);
    }
}
PENNYLANE_RUN_TEST(SWAP)

// NOLINTNEXTLINE: Avoiding complexity errors
template <typename PrecisionT, class GateImplementation> void testApplyCY() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;
    auto ini_st = create_zero_state<PrecisionT>(num_qubits);

    // Test using |+10> state
    GateImplementation::applyHadamard(ini_st.data(), num_qubits, {0}, false);
    GateImplementation::applyPauliX(ini_st.data(), num_qubits, {1}, false);

    CHECK(ini_st == std::vector<ComplexPrecisionT>{
                        Util::ZERO<PrecisionT>(), Util::ZERO<PrecisionT>(),
                        std::complex<PrecisionT>(1.0 / sqrt(2), 0),
                        Util::ZERO<PrecisionT>(), Util::ZERO<PrecisionT>(),
                        Util::ZERO<PrecisionT>(),
                        std::complex<PrecisionT>(1.0 / sqrt(2), 0),
                        Util::ZERO<PrecisionT>()});

    SECTION("CY 0,1 |+10> -> i|100>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(0, -1 / sqrt(2)),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>()};

        auto sv01 = ini_st;
        GateImplementation::applyCY(sv01.data(), num_qubits, {0, 1}, false);
        CHECK(sv01 == expected);
    }

    SECTION("CY 0,2 |+10> -> |010> + i |111>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0.0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(0.0, 1 / sqrt(2))};

        auto sv02 = ini_st;

        GateImplementation::applyCY(sv02.data(), num_qubits, {0, 2}, false);
        CHECK(sv02 == expected);
    }
    SECTION("CY 1,2 |+10> -> i|+11>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(0.0, 1.0 / sqrt(2)),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(0.0, 1 / sqrt(2))};

        auto sv12 = ini_st;

        GateImplementation::applyCY(sv12.data(), num_qubits, {1, 2}, false);
        CHECK(sv12 == expected);
    }
}
PENNYLANE_RUN_TEST(CY)

// NOLINTNEXTLINE: Avoiding complexity errors
template <typename PrecisionT, class GateImplementation> void testApplyCZ() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;

    auto ini_st = create_zero_state<PrecisionT>(num_qubits);

    // Test using |+10> state
    GateImplementation::applyHadamard(ini_st.data(), num_qubits, {0}, false);
    GateImplementation::applyPauliX(ini_st.data(), num_qubits, {1}, false);

    auto st = ini_st;
    CHECK(st == std::vector<ComplexPrecisionT>{
                    Util::ZERO<PrecisionT>(), Util::ZERO<PrecisionT>(),
                    std::complex<PrecisionT>(1.0 / sqrt(2), 0),
                    Util::ZERO<PrecisionT>(), Util::ZERO<PrecisionT>(),
                    Util::ZERO<PrecisionT>(),
                    std::complex<PrecisionT>(1.0 / sqrt(2), 0),
                    Util::ZERO<PrecisionT>()});

    SECTION("CZ0,1 |+10> -> |-10>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(-1 / sqrt(2), 0),
            Util::ZERO<PrecisionT>()};

        auto sv01 = ini_st;
        auto sv10 = ini_st;

        GateImplementation::applyCZ(sv01.data(), num_qubits, {0, 1}, false);
        GateImplementation::applyCZ(sv10.data(), num_qubits, {1, 0}, false);

        CHECK(sv01 == expected);
        CHECK(sv10 == expected);
    }

    SECTION("CZ0,2 |+10> -> |+10>") {
        const std::vector<ComplexPrecisionT> &expected{ini_st};

        auto sv02 = ini_st;
        auto sv20 = ini_st;

        GateImplementation::applyCZ(sv02.data(), num_qubits, {0, 2}, false);
        GateImplementation::applyCZ(sv20.data(), num_qubits, {2, 0}, false);

        CHECK(sv02 == expected);
        CHECK(sv20 == expected);
    }
    SECTION("CZ1,2 |+10> -> |+10>") {
        const std::vector<ComplexPrecisionT> &expected{ini_st};

        auto sv12 = ini_st;
        auto sv21 = ini_st;

        GateImplementation::applyCZ(sv12.data(), num_qubits, {1, 2}, false);
        GateImplementation::applyCZ(sv21.data(), num_qubits, {2, 1}, false);

        CHECK(sv12 == expected);
        CHECK(sv21 == expected);
    }
}
PENNYLANE_RUN_TEST(CZ)

/*******************************************************************************
 * Three-qubit gates
 ******************************************************************************/
template <typename PrecisionT, class GateImplementation>
void testApplyToffoli() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;
    auto ini_st = create_zero_state<PrecisionT>(num_qubits);

    // Test using |+10> state
    GateImplementation::applyHadamard(ini_st.data(), num_qubits, {0}, false);
    GateImplementation::applyPauliX(ini_st.data(), num_qubits, {1}, false);

    SECTION("Toffoli 0,1,2 |+10> -> |010> + |111>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0)};

        auto sv012 = ini_st;

        GateImplementation::applyToffoli(sv012.data(), num_qubits, {0, 1, 2},
                                         false);

        CHECK(sv012 == expected);
    }

    SECTION("Toffoli 1,0,2 |+10> -> |010> + |111>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0)};

        auto sv102 = ini_st;

        GateImplementation::applyToffoli(sv102.data(), num_qubits, {1, 0, 2},
                                         false);

        CHECK(sv102 == expected);
    }

    SECTION("Toffoli 0,2,1 |+10> -> |+10>") {
        const auto &expected = ini_st;

        auto sv021 = ini_st;

        GateImplementation::applyToffoli(sv021.data(), num_qubits, {0, 2, 1},
                                         false);

        CHECK(sv021 == expected);
    }

    SECTION("Toffoli 1,2,0 |+10> -> |+10>") {
        const auto &expected = ini_st;

        auto sv120 = ini_st;
        GateImplementation::applyToffoli(sv120.data(), num_qubits, {1, 2, 0},
                                         false);
        CHECK(sv120 == expected);
    }
}
PENNYLANE_RUN_TEST(Toffoli)

template <typename PrecisionT, class GateImplementation> void testApplyCSWAP() {
    using ComplexPrecisionT = std::complex<PrecisionT>;
    const size_t num_qubits = 3;

    auto ini_st = create_zero_state<PrecisionT>(num_qubits);

    // Test using |+10> state
    GateImplementation::applyHadamard(ini_st.data(), num_qubits, {0}, false);
    GateImplementation::applyPauliX(ini_st.data(), num_qubits, {1}, false);

    SECTION("CSWAP 0,1,2 |+10> -> |010> + |101>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>()};

        auto sv012 = ini_st;
        GateImplementation::applyCSWAP(sv012.data(), num_qubits, {0, 1, 2},
                                       false);
        CHECK(sv012 == expected);
    }

    SECTION("CSWAP 1,0,2 |+10> -> |01+>") {
        std::vector<ComplexPrecisionT> expected{
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            std::complex<PrecisionT>(1.0 / sqrt(2), 0),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>(),
            Util::ZERO<PrecisionT>()};

        auto sv102 = ini_st;
        GateImplementation::applyCSWAP(sv102.data(), num_qubits, {1, 0, 2},
                                       false);
        CHECK(sv102 == expected);
    }
    SECTION("CSWAP 2,1,0 |+10> -> |+10>") {
        const auto &expected = ini_st;

        auto sv210 = ini_st;
        GateImplementation::applyCSWAP(sv210.data(), num_qubits, {2, 1, 0},
                                       false);
        CHECK(sv210 == expected);
    }
}
PENNYLANE_RUN_TEST(CSWAP)
