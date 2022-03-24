// Copyright 2021 Xanadu Quantum Technologies Inc.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/**
 * @file DynamicDispatcher.hpp
 * Defines DynamicDispatcher class. Can be used to call a gate operation by
 * string.
 */

#pragma once

#include "Constant.hpp"
#include "ConstantUtil.hpp"
#include "Error.hpp"
#include "GateUtil.hpp"
#include "KernelType.hpp"
#include "Macros.hpp"
#include "OpToMemberFuncPtr.hpp"
#include "Util.hpp"

#include <cassert>
#include <complex>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

/// @cond DEV
namespace Pennylane::Internal {
/**
 * @brief Register all implemented gates for all available kernels.
 *
 * @tparam PrecisionT Floating point precision of underlying statevector data.
 * @tparam ParamT Floating point type for parameters
 */
template <class PrecisionT, class ParamT> int registerAllAvailableKernels();
} // namespace Pennylane::Internal
/// @endcond

namespace Pennylane {
/**
 * @brief These functions are only used to register kernels to the dynamic
 * dispatcher.
 */
template <class PrecisionT, class ParamT> struct RegisterBeforeMain;

/// @cond DEV
template <> struct RegisterBeforeMain<float, float> {
    const static inline int dummy =
        Internal::registerAllAvailableKernels<float, float>();
};

template <> struct RegisterBeforeMain<double, double> {
    const static inline int dummy =
        Internal::registerAllAvailableKernels<double, double>();
};
/// @endcond

/**
 * @brief DynamicDispatcher class
 *
 * This class calls a gate/generator operation dynamically
 */
template <typename PrecisionT> class DynamicDispatcher {
  public:
    using CFP_t = std::complex<PrecisionT>;

    using GateFunc = std::function<void(
        std::complex<PrecisionT> * /*data*/, size_t /*num_qubits*/,
        const std::vector<size_t> & /*wires*/, bool /*inverse*/,
        const std::vector<PrecisionT> & /*params*/)>;

    using GeneratorFunc = Gates::GeneratorFuncPtrT<PrecisionT>;
    using MatrixFunc = std::function<void(std::complex<PrecisionT> *, size_t,
                                          const std::complex<PrecisionT> *,
                                          const std::vector<size_t> &, bool)>;

  private:
    std::unordered_map<std::string, Gates::GateOperation> str_to_gates_;
    std::unordered_map<std::string, Gates::GeneratorOperation> str_to_gntrs_;

    std::unordered_map<std::pair<Gates::GateOperation, Gates::KernelType>,
                       GateFunc, Util::PairHash>
        gates_;

    std::unordered_map<std::pair<Gates::GeneratorOperation, Gates::KernelType>,
                       GeneratorFunc, Util::PairHash>
        generators_;

    std::unordered_map<std::pair<Gates::MatrixOperation, Gates::KernelType>,
                       MatrixFunc, Util::PairHash>
        matrices_;

    constexpr static auto removeGeneratorPrefix(std::string_view op_name)
        -> std::string_view {
        constexpr std::string_view prefix = "Generator";
        // TODO: change to string::starts_with in C++20
        if (op_name.rfind(prefix) != 0) {
            return op_name;
        }
        return op_name.substr(prefix.size());
    }

    DynamicDispatcher() {
        using Gates::KernelType;

        for (const auto &[gate_op, gate_name] : Gates::Constant::gate_names) {
            str_to_gates_.emplace(gate_name, gate_op);
        }
        for (const auto &[gntr_op, gntr_name] :
             Gates::Constant::generator_names) {
            str_to_gntrs_.emplace(removeGeneratorPrefix(gntr_name), gntr_op);
        }
    }

  public:
    static DynamicDispatcher &getInstance() {
        static DynamicDispatcher singleton;
        return singleton;
    }

    /**
     * @brief Gate name to gate operation
     *
     * @param gate_name Gate name
     */
    [[nodiscard]] auto strToGateOp(const std::string &gate_name) const
        -> Gates::GateOperation {
        return str_to_gates_.at(gate_name);
    }

    /**
     * @brief Generator name to generator operation
     *
     * @param gntr_name Generator name
     */
    [[nodiscard]] auto strToGeneratorOp(const std::string &gntr_name) const
        -> Gates::GeneratorOperation {
        return str_to_gntrs_.at(gntr_name);
    }

    /**
     * @brief Register a new gate operation for the operation. Can pass a custom
     * kernel
     */
    template <typename FunctionType>
    void registerGateOperation(Gates::GateOperation gate_op,
                               Gates::KernelType kernel, FunctionType &&func) {
        // TODO: Add mutex when we go to multithreading
        gates_.emplace(std::make_pair(gate_op, kernel),
                       std::forward<FunctionType>(func));
    }

    /**
     * @brief Register a new gate generator for the operation. Can pass a custom
     * kernel
     */
    template <typename FunctionType>
    void registerGeneratorOperation(Gates::GeneratorOperation gntr_op,
                                    Gates::KernelType kernel,
                                    FunctionType &&func) {
        // TODO: Add mutex when we go to multithreading
        generators_.emplace(std::make_pair(gntr_op, kernel),
                            std::forward<FunctionType>(func));
    }

    /**
     * @brief Register a new matrix operation. Can pass a custom
     * kernel
     */
    // template <typename FunctionType>
    void registerMatrixOperation(Gates::MatrixOperation mat_op,
                                 Gates::KernelType kernel, MatrixFunc func) {
        // FunctionType&& func) {
        // TODO: Add mutex when we go to multithreading
        matrices_.emplace(std::make_pair(mat_op, kernel), func);
    }

    bool isRegistered(Gates::GateOperation gate_op,
                      Gates::KernelType kernel) const {
        return gates_.find(std::make_pair(gate_op, kernel)) != gates_.cend();
    }
    bool isRegistered(Gates::GeneratorOperation gntr_op,
                      Gates::KernelType kernel) const {
        return generators_.find(std::make_pair(gntr_op, kernel)) !=
               generators_.cend();
    }
    bool isRegistered(Gates::MatrixOperation mat_op,
                      Gates::KernelType kernel) const {
        return matrices_.find(std::make_pair(mat_op, kernel)) !=
               matrices_.cend();
    }

    /**
     * @brief Apply a single gate to the state-vector using the given kernel.
     *
     * @param kernel Kernel to run the gate operation.
     * @param data Pointer to data.
     * @param num_qubits Number of qubits.
     * @param op_name Gate operation name.
     * @param wires Wires to apply gate to.
     * @param inverse Indicates whether to use inverse of gate.
     * @param params Optional parameter list for parametric gates.
     */
    void applyOperation(Gates::KernelType kernel, CFP_t *data,
                        size_t num_qubits, const std::string &op_name,
                        const std::vector<size_t> &wires, bool inverse,
                        const std::vector<PrecisionT> &params = {}) const {
        const auto iter =
            gates_.find(std::make_pair(strToGateOp(op_name), kernel));
        if (iter == gates_.cend()) {
            throw std::invalid_argument(
                "The gate " + op_name +
                " is not registered for the given kernel");
        }
        (iter->second)(data, num_qubits, wires, inverse, params);
    }

    /**
     * @brief Apply a single gate to the state-vector using the given kernel.
     *
     * @param kernel Kernel to run the gate operation.
     * @param data Pointer to data.
     * @param num_qubits Number of qubits.
     * @param gate_op Gate operation.
     * @param wires Wires to apply gate to.
     * @param inverse Indicates whether to use inverse of gate.
     * @param params Optional parameter list for parametric gates.
     */
    void applyOperation(Gates::KernelType kernel, CFP_t *data,
                        size_t num_qubits, Gates::GateOperation gate_op,
                        const std::vector<size_t> &wires, bool inverse,
                        const std::vector<PrecisionT> &params = {}) const {
        const auto iter = gates_.find(std::make_pair(gate_op, kernel));
        if (iter == gates_.cend()) {
            throw std::invalid_argument(
                std::string("The gate ") +
                std::string(
                    Util::lookup(Gates::Constant::gate_names, gate_op)) +
                " is not registered for the given kernel");
        }
        (iter->second)(data, num_qubits, wires, inverse, params);
    }

    /**
     * @brief Apply multiple gates to the state-vector using a registered kernel
     *
     * @param data Pointer to data.
     * @param num_qubits Number of qubits.
     * @param ops List of Gate operation names.
     * @param wires List of wires to apply each gate to.
     * @param inverse List of inverses
     * @param params List of parameters
     */
    void
    applyOperations(CFP_t *data, size_t num_qubits,
                    const std::vector<std::string> &ops,
                    const std::vector<std::vector<size_t>> &wires,
                    const std::vector<bool> &inverse,
                    const std::vector<std::vector<PrecisionT>> &params) const {
        const size_t numOperations = ops.size();
        if (numOperations != wires.size() || numOperations != params.size()) {
            throw std::invalid_argument(
                "Invalid arguments: number of operations, wires, and "
                "parameters must all be equal");
        }

        for (size_t i = 0; i < numOperations; i++) {
            applyOperation(data, num_qubits, ops[i], wires[i], inverse[i],
                           params[i]);
        }
    }

    /**
     * @brief Apply multiple (non-parameterized) gates to the state-vector
     * using a registered kernel
     *
     * @param data Pointer to data.
     * @param num_qubits Number of qubits.
     * @param ops List of Gate operation names.
     * @param wires List of wires to apply each gate to.
     * @param inverse List of inverses
     */
    void applyOperations(CFP_t *data, size_t num_qubits,
                         const std::vector<std::string> &ops,
                         const std::vector<std::vector<size_t>> &wires,
                         const std::vector<bool> &inverse) const {
        const size_t numOperations = ops.size();
        if (numOperations != wires.size()) {
            throw std::invalid_argument(
                "Invalid arguments: number of operations, wires, and "
                "parameters must all be equal");
        }

        for (size_t i = 0; i < numOperations; i++) {
            applyOperation(data, num_qubits, ops[i], wires[i], inverse[i], {});
        }
    }

    /**
     * @brief Apply a given matrix directly to the statevector.
     *
     * @param kernel Kernel to use for this operation
     * @param data Pointer to the statevector.
     * @param num_qubits Number of qubits.
     * @param matrix Perfect square matrix in row-major order.
     * @param wires Wires the gate applies to.
     * @param inverse Indicate whether inverse should be taken.
     */
    void applyMatrix(Gates::KernelType kernel, CFP_t *data, size_t num_qubits,
                     const std::complex<PrecisionT> *matrix,
                     const std::vector<size_t> &wires, bool inverse) const {
        using Gates::MatrixOperation;
        assert(num_qubits >= wires.size());

        const auto iter = [n_wires = wires.size(), kernel, this]() {
            switch (n_wires) {
            case 1:
                return matrices_.find(
                    std::make_pair(MatrixOperation::SingleQubitOp, kernel));
            case 2:
                return matrices_.find(
                    std::make_pair(MatrixOperation::TwoQubitOp, kernel));
            default:
                return matrices_.find(
                    std::make_pair(MatrixOperation::MultiQubitOp, kernel));
            }
        }();
        if (iter == matrices_.end()) {
            throw std::invalid_argument(
                std::string(Util::lookup(Gates::Constant::matrix_names,
                                         (iter->first).first)) +
                " is not registered for the given kernel");
        }
        (iter->second)(data, num_qubits, matrix, wires, inverse);
    }

    /**
     * @brief Apply a given matrix directly to the statevector.
     *
     * @param kernel Kernel to use for this operation
     * @param data Pointer to the statevector.
     * @param num_qubits Number of qubits.
     * @param matrix Perfect square matrix in row-major order.
     * @param wires Wires the gate applies to.
     * @param inverse Indicate whether inverse should be taken.
     */
    void applyMatrix(Gates::KernelType kernel, CFP_t *data, size_t num_qubits,
                     const std::vector<std::complex<PrecisionT>> &matrix,
                     const std::vector<size_t> &wires, bool inverse) const {
        if (matrix.size() != Util::exp2(2 * wires.size())) {
            throw std::invalid_argument(
                "The size of matrix does not match with the given "
                "number of wires");
        }
        applyMatrix(kernel, data, num_qubits, matrix.data(), wires, inverse);
    }

    /**
     * @brief Apply a single generator to the state-vector using the given
     * kernel.
     *
     * @param kernel Kernel to run the gate operation.
     * @param data Pointer to data.
     * @param num_qubits Number of qubits.
     * @param gntr_op Generator operation.
     * @param wires Wires to apply gate to.
     * @param adj Indicates whether to use adjoint of gate.
     */
    auto applyGenerator(Gates::KernelType kernel, CFP_t *data,
                        size_t num_qubits, Gates::GeneratorOperation gntr_op,
                        const std::vector<size_t> &wires, bool adj) const
        -> PrecisionT {
        using Gates::Constant::generator_names;
        const auto iter = generators_.find(std::make_pair(gntr_op, kernel));
        if (iter == generators_.cend()) {
            throw std::invalid_argument(
                "Cannot find a gate with a given name \"" +
                std::string(Util::lookup(generator_names, gntr_op)) + "\".");
        }
        return (iter->second)(data, num_qubits, wires, adj);
    }
    /**
     * @brief Apply a single generator to the state-vector using the given
     * kernel.
     *
     * @param kernel Kernel to run the gate operation.
     * @param data Pointer to data.
     * @param num_qubits Number of qubits.
     * @param op_name Gate operation name.
     * @param wires Wires to apply gate to.
     * @param adj Indicates whether to use adjoint of gate.
     */
    auto applyGenerator(Gates::KernelType kernel, CFP_t *data,
                        size_t num_qubits, const std::string &op_name,
                        const std::vector<size_t> &wires, bool adj) const
        -> PrecisionT {
        const auto iter =
            generators_.find(std::make_pair(strToGeneratorOp(op_name), kernel));
        if (iter == generators_.cend()) {
            throw std::invalid_argument(
                "Cannot find a gate with a given name \"" + op_name + "\".");
        }
        return (iter->second)(data, num_qubits, wires, adj);
    }
};
} // namespace Pennylane
