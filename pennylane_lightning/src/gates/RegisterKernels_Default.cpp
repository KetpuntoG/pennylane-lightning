// Copyright 2022 Xanadu Quantum Technologies Inc.

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
 * @file DynamicDispatcher.cpp
 * Register all gate and generator implementations
 */
#include "DynamicDispatcher.hpp"
#include "RegisterKernel.hpp"
#include "Macros.hpp"

#include "cpu_kernels/GateImplementationsLM.hpp"
#include "cpu_kernels/GateImplementationsPI.hpp"
#if PL_USE_OMP
#include "cpu_kernels/GateImplementationsParallelLM.hpp"
#endif
#include "cpu_kernels/QChemGateImplementations.hpp"

namespace Pennylane::Internal {
int registerAllAvailableKernels_Float() {
    registerKernel<float, float, Gates::GateImplementationsLM>();
    registerKernel<float, float, Gates::GateImplementationsPI>();
#if PL_USE_OMP
    registerKernel<float, float, Gates::GateImplementationsParallelLM>();
#endif
    return 1;
}

int registerAllAvailableKernels_Double() {
    registerKernel<double, double, Gates::GateImplementationsLM>();
    registerKernel<double, double, Gates::GateImplementationsPI>();
#if PL_USE_OMP
    registerKernel<double, double, Gates::GateImplementationsParallelLM>();
#endif
    return 1;
}
} // namespace Pennylane::Internal
