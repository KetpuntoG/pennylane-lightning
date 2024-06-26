// Copyright 2018-2023 Xanadu Quantum Technologies Inc.

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
 * @file
 * Statevector simulator where data management resides inside the class.
 */

#pragma once

#include <algorithm> // fill
#include <complex>
#include <vector>

#include "BitUtil.hpp"        // log2PerfectPower, isPerfectPowerOf2
#include "CPUMemoryModel.hpp" // bestCPUMemoryModel
#include "Error.hpp"
#include "KernelType.hpp"
#include "Logger.hpp"
#include "Memory.hpp"
#include "StateVectorLQubit.hpp"
#include "Threading.hpp"
#include "Util.hpp" // exp2

/// @cond DEV
namespace {
using Pennylane::Util::AlignedAllocator;
using Pennylane::Util::bestCPUMemoryModel;
using Pennylane::Util::exp2;
using Pennylane::Util::isPerfectPowerOf2;
using Pennylane::Util::log2PerfectPower;
} // namespace
/// @endcond

namespace Pennylane::LightningQubit {
/**
 * @brief StateVector class where data resides in CPU memory. Memory ownership
 * resides within class.
 *
 * @tparam fp_t Precision data type
 */
template <class fp_t = double>
class StateVectorLQubitManaged final
    : public StateVectorLQubit<fp_t, StateVectorLQubitManaged<fp_t>> {
  public:
    using PrecisionT = fp_t;
    using ComplexT = std::complex<PrecisionT>;
    using CFP_t = ComplexT;
    using MemoryStorageT = Pennylane::Util::MemoryStorageLocation::Internal;

  private:
    using BaseType =
        StateVectorLQubit<PrecisionT, StateVectorLQubitManaged<PrecisionT>>;
    std::vector<ComplexT, AlignedAllocator<ComplexT>> data_;

  public:
    /**
     * @brief Create a new state-vector in the computational basis state |0...0>
     *
     * @param num_qubits Number of qubits
     * @param threading Threading option the state-vector to use
     * @param memory_model Memory model the state-vector will use
     */
    explicit StateVectorLQubitManaged(
        std::size_t num_qubits, Threading threading = Threading::SingleThread,
        CPUMemoryModel memory_model = bestCPUMemoryModel())
        : BaseType{num_qubits, threading, memory_model},
          data_{exp2(num_qubits), ComplexT{0.0, 0.0},
                getAllocator<ComplexT>(this->memory_model_)} {
        LOGGER_INFO("Create a new state-vector with {} qubits", num_qubits);
        LOGGER_DEBUG("num_qubits" + std::to_string(num_qubits) +
                     "threading, memory_model");
        setBasisState(0U);
    }

    /**
     * @brief Construct a state-vector from another state-vector
     *
     * @tparam OtherDerived A derived type of StateVectorLQubit to use for
     * construction.
     * @param other Another state-vector to construct the state-vector from
     */
    template <class OtherDerived>
    explicit StateVectorLQubitManaged(
        const StateVectorLQubit<PrecisionT, OtherDerived> &other)
        : BaseType(other.getNumQubits(), other.threading(),
                   other.memoryModel()),
          data_{other.getData(), other.getData() + other.getLength(),
                getAllocator<ComplexT>(this->memory_model_)} {
        LOGGER_INFO("Construct a state-vector from another state-vector");
        LOGGER_DEBUG("other");
    }

    /**
     * @brief Construct a state-vector from data pointer
     *
     * @param other_data Data pointer to construct the state-vector from.
     * @param other_size Size of the data
     * @param threading Threading option the state-vector to use
     * @param memory_model Memory model the state-vector will use
     */
    StateVectorLQubitManaged(const ComplexT *other_data, std::size_t other_size,
                             Threading threading = Threading::SingleThread,
                             CPUMemoryModel memory_model = bestCPUMemoryModel())
        : BaseType(log2PerfectPower(other_size), threading, memory_model),
          data_{other_data, other_data + other_size,
                getAllocator<ComplexT>(this->memory_model_)} {
        LOGGER_INFO("Construct a state-vector from data pointer");
        LOGGER_DEBUG("other_data, other_size=" + std::to_string(other_size));
        PL_ABORT_IF_NOT(isPerfectPowerOf2(other_size),
                        "The size of provided data must be a power of 2.");
    }

    /**
     * @brief Construct a state-vector from a data vector
     *
     * @tparam Alloc Allocator type of std::vector to use for constructing
     * state-vector.
     * @param other Data to construct the state-vector from
     * @param threading Threading option the state-vector to use
     * @param memory_model Memory model the state-vector will use
     */
    template <class Alloc>
    explicit StateVectorLQubitManaged(
        const std::vector<std::complex<PrecisionT>, Alloc> &other,
        Threading threading = Threading::SingleThread,
        CPUMemoryModel memory_model = bestCPUMemoryModel())
        : StateVectorLQubitManaged(other.data(), other.size(), threading,
                                   memory_model) {
        LOGGER_INFO("Construct a state-vector from a data vector");
        LOGGER_DEBUG("other");
    }

    StateVectorLQubitManaged(const StateVectorLQubitManaged &rhs) = default;
    StateVectorLQubitManaged(StateVectorLQubitManaged &&) noexcept = default;

    StateVectorLQubitManaged &
    operator=(const StateVectorLQubitManaged &) = default;
    StateVectorLQubitManaged &
    operator=(StateVectorLQubitManaged &&) noexcept = default;

    ~StateVectorLQubitManaged() = default;

    /**
     * @brief Prepare a single computational basis state.
     *
     * @param index Index of the target element.
     */
    void setBasisState(const std::size_t index) {
        LOGGER_INFO(
            "Set the state-vector to the computational basis-state at index {}",
            index);
        LOGGER_DEBUG(index);
        std::fill(data_.begin(), data_.end(), 0);
        data_[index] = {1, 0};
    }

    /**
     * @brief Set values for a batch of elements of the state-vector.
     *
     * @param values Values to be set for the target elements.
     * @param indices Indices of the target elements.
     */
    void setStateVector(const std::vector<std::size_t> &indices,
                        const std::vector<ComplexT> &values) {
        LOGGER_INFO("Set values for a batch of elements of the state-vector");
        LOGGER_DEBUG("indices, values");
        for (std::size_t n = 0; n < indices.size(); n++) {
            data_[indices[n]] = values[n];
        }
    }

    /**
     * @brief Reset the data back to the \f$\ket{0}\f$ state.
     *
     */
    void resetStateVector() {
        LOGGER_INFO("Reset the data back to the init state-vector");
        LOGGER_DEBUG("");
        if (this->getLength() > 0) {
            setBasisState(0U);
        }
    }

    [[nodiscard]] auto getData() -> ComplexT * {
        LOGGER_DEBUG("");
        return data_.data();
    }

    [[nodiscard]] auto getData() const -> const ComplexT * {
        LOGGER_DEBUG("");
        return data_.data();
    }

    /**
     * @brief Get underlying data vector
     */
    [[nodiscard]] auto getDataVector()
        -> std::vector<ComplexT, AlignedAllocator<ComplexT>> & {
        LOGGER_DEBUG("");
        return data_;
    }

    [[nodiscard]] auto getDataVector() const
        -> const std::vector<ComplexT, AlignedAllocator<ComplexT>> & {
        LOGGER_DEBUG("");
        return data_;
    }

    /**
     * @brief Update data of the class to new_data
     *
     * @param new_data data pointer to new data.
     * @param new_size size of underlying data storage.
     */
    void updateData(const ComplexT *new_data, std::size_t new_size) {
        LOGGER_INFO("Update data of the class to new_data of size={}",
                    new_size);
        LOGGER_DEBUG("new_data, new_size=" + std::to_string(new_size));
        PL_ASSERT(data_.size() == new_size);
        std::copy(new_data, new_data + new_size, data_.data());
    }

    /**
     * @brief Update data of the class to new_data
     *
     * @tparam Alloc Allocator type of std::vector to use for updating data.
     * @param new_data std::vector contains data.
     */
    template <class Alloc>
    void updateData(const std::vector<ComplexT, Alloc> &new_data) {
        LOGGER_DEBUG("new_data");
        updateData(new_data.data(), new_data.size());
    }

    AlignedAllocator<ComplexT> allocator() const {
        LOGGER_INFO("Get the allocator");
        LOGGER_DEBUG("");
        return data_.get_allocator();
    }
};
} // namespace Pennylane::LightningQubit