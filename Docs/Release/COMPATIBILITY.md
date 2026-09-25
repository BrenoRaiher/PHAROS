# PHAROS Compatibility Policy

This document defines the file and extension contracts supported by PHAROS
1.0. A later release may broaden this table, but it must not silently replace
an unsupported saved-scenario library.

## Saved Scenario Library

PHAROS 1.0 reads and writes library format `1`. The primary Windows library is
stored under:

`%LOCALAPPDATA%\PHAROS\Saved\SaveGames\TGSimulationLibrary.sav`

Before replacing an existing library during a normal save, PHAROS preserves a
copy under `SaveGames\Backups`. The newest 10 routine backups are retained.

If the existing library cannot be read or uses an unsupported format, PHAROS
first copies the original into `SaveGames\Recovery`. Recovery copies are not
automatically pruned. If that copy cannot be created, PHAROS refuses to
replace the existing library.

A fresh PHAROS user-data folder starts with an empty scenario library and no
saved results. PHAROS does not automatically import data from other applications
or development installations. Reinstalling or upgrading PHAROS preserves its
existing user data.

## TGSCN

PHAROS imports and exports the TGSCN file contract documented in
[the TGSCN reference](https://github.com/BrenoRaiher/PHAROS/blob/main/Docs/Reference/TGSCN.md). Rotational joint coordinates, rates, and limits are
authored in degrees while PHAROS uses radians internally.

## Saved Simulation Results

PHAROS 1.0 stores result metadata in format `1`. Each completed run also keeps
its exported TGSCN snapshot and CSV result together under
`%LOCALAPPDATA%\PHAROS\Saved\SimulationResults`.

Visualization and continuation require both the result CSV and the associated
scenario snapshot. Moving or deleting either file can make the saved run
unavailable from the Scenario Library.

## Controller DLLs

PHAROS requires the PHAROS Controller API defined by the Controller SDK
distributed with the application. Before loading a DLL, PHAROS validates its
required exports and reported public-structure sizes. A controller that does
not match this contract must be rebuilt.

Each thruster command carries an optional analytic derivative of actual
outward propellant discharge. It is part of the required output structure and
is written during `PHAROS_ComputeControl`.

Controller source and built DLLs are stored under
`%LOCALAPPDATA%\PHAROS\Saved\Controllers`.

## Contract Changes

- PHAROS supports the TGSCN and Controller API contracts distributed with the
  installed application.
- Scenario files and controllers must match the installed PHAROS release.
- Back up scenario libraries, TGSCN files, controller source, and important
  results before upgrading.
