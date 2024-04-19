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
 * Defines a class for the measurement of observables in quantum states
 * represented by a Lightning Tensor MPS class.
 */

#pragma once

#include <algorithm>
#include <complex>
#include <cuda.h>
#include <cutensornet.h>
#include <random>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Observables.hpp"
#include "Observables_cuMPS.hpp"
#include "cuGateTensorCache.hpp"
#include "cuMPS.hpp"
#include "cuTensorNetError.hpp"
#include "cuda_helpers.hpp"

/// @cond DEV
namespace {
using namespace Pennylane;
using namespace Pennylane::Measures;
using namespace Pennylane::Observables;
using namespace Pennylane::LightningTensor::Observables;
namespace cuUtil = Pennylane::LightningGPU::Util;
using Pennylane::LightningTensor::cuMPS;
using namespace Pennylane::LightningTensor::Util;
using namespace Pennylane::Util;
} // namespace
/// @endcond

namespace Pennylane::LightningTensor::Measures {
/**
 * @brief Observable's Measurement Class.
 *
 * This class couples with a statevector to performs measurements.
 * Observables are defined by its operator(matrix), the observable class,
 * or through a string-based function dispatch.
 *
 * @tparam cuMPS type of the statevector to be measured.
 */
template <class TensorNetT>
class Measurements {
  private:
    using PrecisionT = typename TensorNetT::PrecisionT;
    using ComplexT = typename TensorNetT::ComplexT;
    using CFP_t = decltype(cuUtil::getCudaType(PrecisionT{}));
    cudaDataType_t data_type_;
    TensorNetT &_statetensor;

  public:
    explicit Measurements(TensorNetT &statetensor)
        : _statetensor{statevector} {
        if constexpr (std::is_same_v<CFP_t, cuDoubleComplex> ||
                      std::is_same_v<CFP_t, double2>) {
            data_type_ = CUDA_C_64F;
        } else {
            data_type_ = CUDA_C_32F;
        }
    };

    /**
     * @brief Expected value of an observable.
     *
     * @param operation String with the operator name.
     * @param wires Wires where to apply the operator.
     * @return Floating point expected value of the observable.
     */
    auto expval(const std::string &operation, const std::vector<size_t> &wires)
        -> PrecisionT {
        std::vector<PrecisionT> params = {0.0};
        std::vector<ComplexT> gate_matrix = {};
        return this->expval_(operation, wires, params, gate_matrix);
    }

    /**
     * @brief Calculate expectation value for a general Observable.
     *
     * @param ob Observable.
     * @return Expectation value with respect to the given observable.
     */
    auto expval(const ObservableCudaTN<PrecisionT> &ob) -> PrecisionT {
      
    }

}; // class Measurements
} // namespace Pennylane::LightningTensor::Measures