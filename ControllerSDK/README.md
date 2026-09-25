# PHAROS Controller SDK

For the complete standalone workflow and examples that command thrusters,
articulated DOFs, reaction wheels, and external body torque, see the
[PHAROS Standalone Runner Guide](https://github.com/BrenoRaiher/PHAROS/blob/main/Docs/GettingStarted/StandaloneRunner.md).

This directory is the complete source-level contract for controller DLLs loaded
by the packaged PHAROS application.

- `PHAROSControllerAPI.h` is the public, POD-only controller API.
- `TGControllerAPI.h` contains the internal C ABI definitions used by that
  public header.
- `ControllerTemplate.cpp` is copied into a user-writable controller workspace.
- User DLLs do not include Unreal or simulation-engine headers and do not
  rebuild PHAROS.
- All vectors and matrices use SI units and the frame conventions named by each
  field. `PHAROSMat3` is row-major.
- Input pointers are valid only during one host evaluation.
- Output storage belongs to the host and is initialized to zero before each call.
- `PHAROS_NextControllerDiscontinuityElapsedTime` is an optional export used to
  announce hard time switches so Runge-Kutta steps end exactly at them. The
  supplied template already exports it.
- Every `PHAROSThrusterCommand` contains the optional analytic total derivative
  of that commanded thruster's actual outward propellant discharge in kg/s^2.
  It is populated in `PHAROS_ComputeControl`, is not another thrust command,
  and is cleared before every call.

PHAROS validates the public structure sizes reported by each DLL before loading
it. A controller that does not match the Controller SDK distributed with the
application must be rebuilt. An omitted mass-flow derivative contributes zero
only to the corresponding geometric-center-of-mass second-mass-derivative
correction; ordinary thrust and propellant depletion remain active.
