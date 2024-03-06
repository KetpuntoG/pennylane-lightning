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
"""
Class implementation for state vector measurements.
"""

# pylint: disable=import-error, no-name-in-module, ungrouped-imports
try:
    from pennylane_lightning.lightning_qubit_ops import (
        MeasurementsC64,
        MeasurementsC128,
    )
except ImportError:
    pass

from typing import Callable, List

import numpy as np
import pennylane as qml
from pennylane.measurements import (
    CountsMP,
    ExpectationMP,
    MeasurementProcess,
    ProbabilityMP,
    SampleMeasurement,
    Shots,
    StateMeasurement,
    VarianceMP,
)
from pennylane.tape import QuantumScript
from pennylane.typing import Result, TensorLike
from pennylane.wires import Wires

from pennylane_lightning.core._serialize import QuantumScriptSerializer

from ._state_vector import LightningStateVector


class LightningMeasurements:
    """Lightning Measurements class

    Measures the state provided by the LightningStateVector class.

    Args:
        qubit_state(LightningStateVector): Lightning state-vector class containing the state vector to be measured.
    """

    def __init__(self, qubit_state: LightningStateVector) -> None:
        self._qubit_state = qubit_state
        self._state = qubit_state.state_vector
        self._dtype = qubit_state.dtype
        self._measurement_lightning = self._measurement_dtype()(self.state)

    @property
    def qubit_state(self):
        """Returns a handle to the LightningStateVector class."""
        return self._qubit_state

    @property
    def state(self):
        """Returns a handle to the Lightning internal data class."""
        return self._state

    @property
    def dtype(self):
        """Returns the simulation data type."""
        return self._dtype

    def _measurement_dtype(self):
        """Binding to Lightning Measurements C++ class.

        Returns: the Measurements class
        """
        return MeasurementsC64 if self.dtype == np.complex64 else MeasurementsC128

    def state_diagonalizing_gates(self, measurementprocess: StateMeasurement) -> TensorLike:
        """Apply a measurement to state when the measurement process has an observable with diagonalizing gates.
            This method is bypassing the measurement process to default.qubit implementation.

        Args:
            measurementprocess (StateMeasurement): measurement to apply to the state

        Returns:
            TensorLike: the result of the measurement
        """
        diagonalizing_gates = measurementprocess.diagonalizing_gates()
        self._qubit_state.apply_operations(measurementprocess.diagonalizing_gates())
        state_array = self._qubit_state.state
        wires = Wires(range(self._qubit_state.num_wires))

        result = measurementprocess.process_state(state_array, wires)

        self._qubit_state.apply_operations([qml.adjoint(g) for g in reversed(diagonalizing_gates)])

        return result

    # pylint: disable=protected-access
    def expval(self, measurementprocess: MeasurementProcess):
        """Expectation value of the supplied observable contained in the MeasurementProcess.

        Args:
            measurementprocess (StateMeasurement): measurement to apply to the state

        Returns:
            Expectation value of the observable
        """

        if measurementprocess.obs.name == "SparseHamiltonian":
            # ensuring CSR sparse representation.
            CSR_SparseHamiltonian = measurementprocess.obs.sparse_matrix(
                wire_order=list(range(self._qubit_state.num_wires))
            ).tocsr(copy=False)
            return self._measurement_lightning.expval(
                CSR_SparseHamiltonian.indptr,
                CSR_SparseHamiltonian.indices,
                CSR_SparseHamiltonian.data,
            )

        if (
            measurementprocess.obs.name in ["Hamiltonian", "Hermitian"]
            or (measurementprocess.obs.arithmetic_depth > 0)
            or isinstance(measurementprocess.obs.name, List)
        ):
            ob_serialized = QuantumScriptSerializer(
                self._qubit_state.device_name, self.dtype == np.complex64
            )._ob(measurementprocess.obs)
            return self._measurement_lightning.expval(ob_serialized)

        return self._measurement_lightning.expval(
            measurementprocess.obs.name, measurementprocess.obs.wires
        )

    # pylint: disable=protected-access
    def probs(self, measurementprocess: MeasurementProcess):
        """Probabilities of the supplied observable or wires contained in the MeasurementProcess.

        Args:
            measurementprocess (StateMeasurement): measurement to apply to the state

        Returns:
            Probabilities of the supplied observable or wires
        """
        diagonalizing_gates = measurementprocess.diagonalizing_gates()
        if diagonalizing_gates:
            self._qubit_state.apply_operations(diagonalizing_gates)
        results = self._measurement_lightning.probs(measurementprocess.wires.tolist())
        if diagonalizing_gates:
            self._qubit_state.apply_operations(
                [qml.adjoint(g) for g in reversed(diagonalizing_gates)]
            )
        return results

    # pylint: disable=protected-access
    def var(self, measurementprocess: MeasurementProcess):
        """Variance of the supplied observable contained in the MeasurementProcess.

        Args:
            measurementprocess (StateMeasurement): measurement to apply to the state

        Returns:
            Variance of the observable
        """

        if measurementprocess.obs.name == "SparseHamiltonian":
            # ensuring CSR sparse representation.
            CSR_SparseHamiltonian = measurementprocess.obs.sparse_matrix(
                wire_order=list(range(self._qubit_state.num_wires))
            ).tocsr(copy=False)
            return self._measurement_lightning.var(
                CSR_SparseHamiltonian.indptr,
                CSR_SparseHamiltonian.indices,
                CSR_SparseHamiltonian.data,
            )

        if (
            measurementprocess.obs.name in ["Hamiltonian", "Hermitian"]
            or (measurementprocess.obs.arithmetic_depth > 0)
            or isinstance(measurementprocess.obs.name, List)
        ):
            ob_serialized = QuantumScriptSerializer(
                self._qubit_state.device_name, self.dtype == np.complex64
            )._ob(measurementprocess.obs)
            return self._measurement_lightning.var(ob_serialized)

        return self._measurement_lightning.var(
            measurementprocess.obs.name, measurementprocess.obs.wires
        )

    def get_measurement_function(
        self, measurementprocess: MeasurementProcess
    ) -> Callable[[MeasurementProcess, TensorLike], TensorLike]:
        """Get the appropriate method for performing a measurement.

        Args:
            measurementprocess (MeasurementProcess): measurement process to apply to the state

        Returns:
            Callable: function that returns the measurement result
        """
        if isinstance(measurementprocess, StateMeasurement):
            if isinstance(measurementprocess, ExpectationMP):
                if measurementprocess.obs.name in [
                    "Identity",
                    "Projector",
                ]:
                    return self.state_diagonalizing_gates
                return self.expval

            if isinstance(measurementprocess, ProbabilityMP):
                return self.probs

            if isinstance(measurementprocess, VarianceMP):
                if measurementprocess.obs.name in [
                    "Identity",
                    "Projector",
                ]:
                    return self.state_diagonalizing_gates
                return self.var
            if measurementprocess.obs is None or measurementprocess.obs.has_diagonalizing_gates:
                return self.state_diagonalizing_gates

        raise NotImplementedError

    def measurement(self, measurementprocess: MeasurementProcess) -> TensorLike:
        """Apply a measurement process to a state.

        Args:
            measurementprocess (MeasurementProcess): measurement process to apply to the state

        Returns:
            TensorLike: the result of the measurement
        """
        return self.get_measurement_function(measurementprocess)(measurementprocess)

    def measure_final_state(self, circuit: QuantumScript) -> Result:
        """
        Perform the measurements required by the circuit on the provided state.

        This is an internal function that will be called by the successor to ``lightning.qubit``.

        Args:
            circuit (QuantumScript): The single circuit to simulate

        Returns:
            Tuple[TensorLike]: The measurement results
        """

        if not circuit.shots:
            # analytic case
            if len(circuit.measurements) == 1:
                return self.measurement(circuit.measurements[0])

            return tuple(self.measurement(mp) for mp in circuit.measurements)

        # finite-shot case
        results = tuple(
            self.measure_with_samples(
                mp,
                shots=circuit.shots,
            )
            for mp in circuit.measurements
        )

        if len(circuit.measurements) == 1:
            if circuit.shots.has_partitioned_shots:
                return tuple(res[0] for res in results)

            return results[0]

        return results

    def measure_with_samples(
        self,
        measurementprocess: SampleMeasurement,
        shots: Shots,
    ) -> TensorLike:
        """
        Returns the samples of the measurement process performed on the given state,
        by rotating the state into the measurement basis using the diagonalizing gates
        given by the measurement process.

        Args:
            mp (~.measurements.SampleMeasurement): The sample measurement to perform
            state (np.ndarray[complex]): The state vector to sample from
            shots (~.measurements.Shots): The number of samples to take
            is_state_batched (bool): whether the state is batched or not
            rng (Union[None, int, array_like[int], SeedSequence, BitGenerator, Generator]): A
                seed-like parameter matching that of ``seed`` for ``numpy.random.default_rng``.
                If no value is provided, a default RNG will be used.
            prng_key (Optional[jax.random.PRNGKey]): An optional ``jax.random.PRNGKey``. This is
                the key to the JAX pseudo random number generator. Only for simulation using JAX.

        Returns:
            TensorLike[Any]: Sample measurement results
        """
        diagonalizing_gates = measurementprocess.diagonalizing_gates()
        if diagonalizing_gates:
            self._qubit_state.apply_operations(diagonalizing_gates)

        wires = measurementprocess.wires

        def _process_single_shot(samples):
            res = measurementprocess.process_samples(samples, wires)
            return res if isinstance(measurementprocess, CountsMP) else qml.math.squeeze(res)

        # if there is a shot vector, build a list containing results for each shot entry
        if shots.has_partitioned_shots:
            processed_samples = []
            for s in shots:
                # currently we call sample_state for each shot entry, but it may be
                # better to call sample_state just once with total_shots, then use
                # the shot_range keyword argument
                try:
                    samples = self._measurement_lightning.generate_samples(len(wires), s).astype(
                        int, copy=False
                    )
                except ValueError as e:
                    if str(e) != "probabilities contain NaN":
                        raise e
                    samples = qml.math.full((s, len(wires)), 0)

                processed_samples.append(_process_single_shot(samples))
            if diagonalizing_gates:
                self._qubit_state.apply_operations(
                    [qml.adjoint(g) for g in reversed(diagonalizing_gates)]
                )

            return tuple(zip(*processed_samples))

        try:
            samples = self._measurement_lightning.generate_samples(
                len(wires), shots.total_shots
            ).astype(int, copy=False)
        except ValueError as e:
            if str(e) != "probabilities contain NaN":
                raise e
            samples = qml.math.full((shots.total_shots, len(wires)), 0)

        if diagonalizing_gates:
            self._qubit_state.apply_operations(
                [qml.adjoint(g) for g in reversed(diagonalizing_gates)]
            )
        return _process_single_shot(samples)
