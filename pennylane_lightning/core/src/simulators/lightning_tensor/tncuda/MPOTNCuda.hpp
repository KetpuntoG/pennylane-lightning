// Copyright 2024 Xanadu Quantum Technologies Inc.

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
 * @file MPOTNCuda.hpp
 * Base class for cuTensorNet-backed MPO.
 */

#pragma once

#include <algorithm>
#include <cuComplex.h>
#include <cutensornet.h>
#include <queue>
#include <vector>

#include "TensorCuda.hpp"
#include "tncudaError.hpp"
#include "tncuda_helpers.hpp"

namespace Pennylane::LightningTensor::TNCuda {

/**
 * @brief Class for MPO tensor network in cuTensorNet backend.
 * Any gate tensor can be represented as a MPO tensor network in the context of
 MPS.
 * The gate tensor has to be decomposed respect to its target wires. If the
 target wires
 * are not adjacent, Identity tensors are inserted between the MPO tensors.
 * 1. The MPO tensors' modes order in an open boundary condition are:
   2              3              2
   |              |              |
   X--1--....--0--X--2--....--0--X
   |              |              |
   0              1              1

 * 2. The extents of the left side bound MPO tensor are [2, bondR, 2].
   The extents of the right side bound MPO tensor are [bondL, 2, 2].
   The extents of the middle MPO tensors are [bondL, 2, bondR, 2].

 * MPO tensor modes with connecting Identity tensors in an open boundary
 condition are:

   X--I--...--I--X--I--...--I--X
 * Note that extents of mode 0 and 2 of I are equal to the bond dimension of
 * the nearest MPO tensor. The shape of a connecting Identity tensor is
 *[bond, 2, bond, 2]. If the Identity tensor is flatten, its 0th and
 * (2*bind+1)th element are complex{1, 0}, and the rest are complex{0,0}.
 * Also note that the life time of the tensor data is designed to be aligned
 with
 * the life time of the tensor network it's applied to.
 * @tparam TensorNetT Tensor network type.
 */
template <class TensorNetT> class MPOTNCuda {
  private:
    using ComplexT = typename TensorNetT::ComplexT;
    using PrecisionT = typename TensorNetT::PrecisionT;
    using CFP_t = typename TensorNetT::CFP_t;

    const TensorNetT &tensor_network_;
    std::vector<std::size_t> wires_; // pennylane  wires convention

    // To buuld a MPO tensor network, we need: 1. a cutensornetHandle; 2. a
    // cutensornetNetworkOperator_t object; 3. Complex coefficient associated
    // with the appended operator component. 4. Number of MPO sites; 5. MPO
    // tensor mode extents for each MPO tensor;
    // 6. Boundary conditions;
    cutensornetNetworkOperator_t networkOperator_;
    cuDoubleComplex coeff_{1.0, 0.0}; // default coefficient
    cutensornetBoundaryCondition_t boundaryCondition_{
        CUTENSORNET_BOUNDARY_CONDITION_OPEN}; // open boundary condition
    int64_t componentIdx_;

    std::size_t maxBondDim_;
    std::size_t numSites_;
    std::vector<std::size_t> stateSitesExtents_;
    std::vector<int64_t> stateSitesExtents_int64_;
    std::vector<std::size_t> modes_;
    std::vector<int32_t> modes_int32_;

    std::vector<std::size_t> bondDims_;

    std::vector<std::vector<std::size_t>> modesExtents_;
    std::vector<std::vector<int64_t>> modesExtents_int64_;
    std::vector<TensorCuda<PrecisionT>> tensors_;

    /**
     * @brief Get a vector of pointers to extents of each site.
     *
     * @return std::vector<int64_t const *> Note int64_t const* is
     * required by cutensornet backend.
     */
    [[nodiscard]] auto getSitesExtentsPtr_() -> std::vector<int64_t const *> {
        std::vector<int64_t const *> sitesExtentsPtr_int64(numSites_);
        for (std::size_t i = 0; i < numSites_; i++) {
            sitesExtentsPtr_int64[i] = modesExtents_int64_[i].data();
        }
        return sitesExtentsPtr_int64;
    }

    /**
     * @brief Get a vector of pointers to tensor data of each site.
     *
     * @return std::vector<uint64_t *>
     */
    [[nodiscard]] auto getTensorsDataPtr() -> std::vector<uint64_t *> {
        std::vector<uint64_t *> tensorsDataPtr(numSites_);
        for (std::size_t i = 0; i < numSites_; i++) {
            tensorsDataPtr[i] = reinterpret_cast<uint64_t *>(
                tensors_[i].getDataBuffer().getData());
        }
        return tensorsDataPtr;
    }

  public:
    explicit MPOTNCuda(const TensorNetT &tensor_network,
                       const std::vector<std::vector<ComplexT>> &tensors,
                       const std::vector<std::size_t> &wires,
                       const std::size_t maxBondDim)
        : tensor_network_(tensor_network) {
        PL_ABORT_IF_NOT(tensors.size() == wires.size(),
                        "Number of tensors and wires must match.");

        PL_ABORT_IF(maxBondDim < 2,
                    "Max MPO bond dimension must be at least 2.");

        PL_ABORT_IF(std::is_sorted(wires.begin(), wires.end()),
                    "Only sorted target wires is accepeted.");

        wires_ = wires;

        // set up max bond dimensions and number of MPO sites
        maxBondDim_ = maxBondDim;
        numSites_ = wires.back() - wires.front() + 1;

        stateSitesExtents_ = std::vector<std::size_t>(numSites_, 2);
        stateSitesExtents_int64_ = std::vector<int64_t>(numSites_, 2);

        // set up MPO target modes
        for (std::size_t i = 0; i < numSites_; ++i) {
            modes_.push_back(wires.front() + i);
            modes_int32_.push_back(static_cast<int32_t>(wires.front() + i));
        }

        // set up target bond dimensions
        std::vector<std::size_t> targetSitesBondDims =
            std::vector<std::size_t>(wires.size() - 1, maxBondDim);
        for (std::size_t i = 0; i < targetSitesBondDims.size(); i++) {
            std::size_t bondDim =
                std::min(i + 1, targetSitesBondDims.size() - i) *
                2; // 1+1 (1 for bra and 1 for ket)
            if (bondDim <= log2(maxBondDim_)) {
                targetSitesBondDims[i] = (std::size_t{1} << bondDim);
            }
        }

        bondDims_ = targetSitesBondDims;

        // Insert bond dimensions of Identity tensors
        if (wires.size() != numSites_) {
            for (std::size_t i = 0; i < wires.size() - 1; i++) {
                const std::size_t numIdentitySites =
                    wires[i + 1] - wires[i] - 1;
                if (numIdentitySites > 0) {
                    std::vector<std::size_t> identitySites(
                        numIdentitySites, targetSitesBondDims[i]);
                    bondDims_.insert(bondDims_.begin() + i + 1,
                                     identitySites.begin(),
                                     identitySites.end());
                }
            }
        }
        // set up MPO tensor mode extents and initialize MPO tensors
        for (std::size_t i = 0; i < numSites_; i++) {
            std::vector<std::size_t> siteModes;
            if (i == 0) {
                modesExtents_.push_back({2, bondDims_[i], 2});
                modesExtents_int64_.push_back(
                    {2, static_cast<int64_t>(bondDims_[i]), 2});
                siteModes = std::vector<std::size_t>{
                    wires_[0] + i, wires_[0] + numSites_ + i,
                    wires_[0] + 2 * numSites_ + i};
            } else if (i == numSites_ - 1) {
                modesExtents_.push_back({bondDims_[i - 1], 2, 2});
                modesExtents_int64_.push_back(
                    {static_cast<int64_t>(bondDims_[i - 1]), 2, 2});
                siteModes = std::vector<std::size_t>{
                    wires_[0] + numSites_ + i, wires_[0] + i,
                    wires_[0] + 2 * numSites_ + i};

            } else {
                modesExtents_.push_back({bondDims_[i], 2, bondDims_[i], 2});
                modesExtents_int64_.push_back(
                    {static_cast<int64_t>(bondDims_[i]), 2,
                     static_cast<int64_t>(bondDims_[i]), 2});
                siteModes = std::vector<std::size_t>{
                    wires_[0] + numSites_ + i, wires_[0] + i,
                    wires_[0] + numSites_ + i + 1,
                    wires_[0] + 2 * numSites_ + i};
            }
            tensors_.emplace_back(siteModes.size(), siteModes,
                                  modesExtents_.back(),
                                  tensor_network_.getDevTag());
            tensors_.back().getDataBuffer().zeroInit();
        }

        // set up MPO tensor data
        std::queue<std::size_t> wires_queue;
        for (std::size_t i = 0; i < wires.size(); i++) {
            wires_queue.push(wires[i]);
        }

        for (std::size_t i = 0; i < tensors_.size(); i++) {
            if (wires_queue.front() == (wires_[0] + i)) {
                tensors_[i].getDataBuffer().CopyHostDataToGpu(
                    tensors[i].data(), tensors[i].size());
                wires_queue.pop();
            } else {
                CFP_t value_cu =
                    cuUtil::complexToCu<ComplexT>(ComplexT{1.0, 0.0});
                std::size_t target_idx = 0;
                PL_CUDA_IS_SUCCESS(cudaMemcpy(
                    &tensors_[i].getDataBuffer().getData()[target_idx],
                    &value_cu, sizeof(CFP_t), cudaMemcpyHostToDevice));

                target_idx = 2 * bondDims_[i - 1] *
                             2; // i - 1 will be always non-negative
                PL_CUDA_IS_SUCCESS(cudaMemcpy(
                    &tensors_[i].getDataBuffer().getData()[target_idx],
                    &value_cu, sizeof(CFP_t), cudaMemcpyHostToDevice));
            }
        }

        // set up MPO tensor network operator
        PL_CUTENSORNET_IS_SUCCESS(cutensornetCreateNetworkOperator(
            /* const cutensornetHandle_t */ tensor_network_.getTNCudaHandle(),
            /* int32_t */ static_cast<int32_t>(numSites_),
            /* const int64_t stateModeExtents */
            stateSitesExtents_int64_.data(),
            /* cudaDataType_t */ tensor_network_.getCudaDataType(),
            /* */ &networkOperator_));

        // append MPO tensor network operator components
        PL_CUTENSORNET_IS_SUCCESS(cutensornetNetworkOperatorAppendMPO(
            /* const cutensornetHandle_t */ tensor_network_.getTNCudaHandle(),
            /* cutensornetNetworkOperator_t */ networkOperator_,
            /* const cuDoubleComplex */ coeff_,
            /* int32_t numStateModes */ static_cast<int32_t>(numSites_),
            /* const int32_t stateModes[] */ modes_int32_.data(),
            /* const int64_t *stateModeExtents[] */
            getSitesExtentsPtr_().data(),
            /* const int64_t *tensorModeStrides[] */ nullptr,
            /* const void * */
            const_cast<const void **>(
                reinterpret_cast<void **>(getTensorsDataPtr().data())),
            /* cutensornetBoundaryCondition_t */ boundaryCondition_,
            /* int64_t * */ &componentIdx_));
    }

    ~MPOTNCuda() {
        PL_CUTENSORNET_IS_SUCCESS(
            cutensornetDestroyNetworkOperator(networkOperator_));
    };
};
} // namespace Pennylane::LightningTensor::TNCuda