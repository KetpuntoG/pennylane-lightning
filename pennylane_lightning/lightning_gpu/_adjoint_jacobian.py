# Copyright 2018-2024 Xanadu Quantum Technologies Inc.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
r"""
Internal methods for adjoint Jacobian differentiation method.
"""

import numpy as np
import pennylane as qml

from ._state_vector import LightningGPUStateVector


class LightningGPUAdjointJacobian:
    """Check and execute the adjoint Jacobian differentiation method.

    Args:
        qubit_state(LightningGPUStateVector): State Vector to calculate the adjoint Jacobian with.
        batch_obs(bool): If serialized tape is to be batched or not.
    """

    def __init__(self, lgpu_state: LightningGPUStateVector, batch_obs: bool = False) -> None:
        self._qubit_state = lgpu_state
        self._state = lgpu_state.state_vector
        self._dtype = lgpu_state.dtype
        self._batch_obs = batch_obs

    @property
    def qubit_state(self):
        """Returns a handle to the LightningGPUStateVector class."""
        return self._qubit_state

    @property
    def state(self):
        """Returns a handle to the Lightning internal data class."""
        return self._state

    @property
    def dtype(self):
        """Returns the simulation data type."""
        return self._dtype
