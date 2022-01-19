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
 * @file KernelType.hpp
 * Defines possible kernel types as enum and define python export.
 */
#pragma once
#include "Error.hpp"
#include "Util.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace Pennylane {
/**
 * @brief Define kernel id for each implemenation
 */
enum class KernelType { PI, LM, None };
} // namespace Pennylane

namespace Pennylane::Constant {
[[maybe_unused]] constexpr std::array kernels_to_pyexport = {KernelType::PI,
                                                             KernelType::LM};
} // namespace Pennylane::Constant
