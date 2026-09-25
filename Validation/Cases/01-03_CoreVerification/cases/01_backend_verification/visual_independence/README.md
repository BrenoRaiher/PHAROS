# DAT-02: independence from visual appearance

This dedicated Case 1 check uses the current PHAROS standalone runner. It does not use mission display variants.

Five TGSCN files describe a rotating, fixed two-component spacecraft with prescribed propulsion. The reference contains a 100 kg bus and a 1 kg propellant component, a 2 N thrust interval from 0 to 2 s, and a 3 s propagation. The initial body angular velocity is (0.01, 0.02, -0.03) rad/s. RK4 uses a 0.01 s maximum step and the output interval is 0.1 s.

The three visual variants change only the `[components.visual]` tables:

- `primitives.tgscn`: visible colored boxes with prescribed display dimensions.
- `transformed.tgscn`: colored cylinders, different dimensions, visual position, orientation, and scale.
- `hidden.tgscn`: spheres with altered visual transforms and visibility disabled.

The script parses each TGSCN independently with Python's TOML reader and requires that removing the visual tables leaves identical input documents. It then runs each file and requires identical column layouts and numerical histories, including a byte-for-byte comparison of the complete solution CSV. All recorded values must be finite. The reference motion must change during propagation. The fifth file, `physical_control.tgscn`, changes the actual thrust from 2 N to 3 N and must produce a different history.

All three visual comparisons passed. Each solution contains 31 samples and 111 columns, and the physical control produces a different history. `metrics.json` records the runner, input, solution, and summary hashes. The runner hash also matched the executable in the current PHAROS project when these tests were executed.

## Reproduction

Run in a working copy of the validation package with Python 3.11 or newer:

```powershell
python Cases/01-03_CoreVerification/cases/01_backend_verification/visual_independence/verify.py
```

The default runtime is the package's `Support/Runtime` folder; `--runtime PATH` selects another explicitly. The script overwrites this case's `results` and `metrics.json` and fails if any acceptance condition is not satisfied. No controller DLL, imported mesh, texture, or profile file is required.

This check covers the tested visual metadata in standalone scenario execution. It does not exercise interactive rendering, camera operations, mesh-derived generation of physical force geometry, or cancellation. Radiation, gravity, and atmospheric models are disabled in these constructed inputs.
