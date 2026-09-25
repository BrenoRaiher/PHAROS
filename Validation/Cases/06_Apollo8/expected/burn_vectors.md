# Retained Burn Directions and Magnitudes

The `burn_vectors.json` file supplies normalized fixed ICRF directions for the three midcourse corrections and transearth injection. MC2 and TEI include bounded calibration adjustments. The first and third corrections retain their inherited directions. LOI and circularization execute live Moon-relative laws; their stored fixed vectors are fallbacks, not the primary pointing law.

The authoritative retained scalar overrides are in `controller_tuning.json`. Unchanged settings remain explicit in `../tools/continuous_chain.py`. The executed values are recorded beside each DLL in `segments/XX/controllers/controller_parameters.json`; the final package audit checks agreement with the generator.

An impulse inferred from a truth pair, a historical reported scalar delta-v, the controller's effective scalar target, the integral of thrust-acceleration magnitude, and the magnitude of its vector integral are distinct quantities. The validation report documents which is being compared. The inherited first-correction 24.8 ft/s effective target is retained despite conflicting historical reports of achieved impulse.
