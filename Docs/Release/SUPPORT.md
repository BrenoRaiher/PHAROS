# PHAROS Support

Support and maintenance are voluntary. Use the repository issue tracker to
report reproducible defects, suggest features, and request documentation
corrections:

https://github.com/BrenoRaiher/PHAROS/issues

Include the following when reporting a defect:

- PHAROS version and whether the build is packaged or editor-based.
- Windows version, processor, graphics adapter, graphics-driver version, and
  display scaling.
- Exact steps and the expected and observed behavior.
- A minimal TGSCN or scenario configuration when it can be shared safely.
- Relevant local log excerpts.

Packaged Windows data is normally stored below:

`%LOCALAPPDATA%\PHAROS\Saved`

Useful subdirectories include `Logs`, `SaveGames`, `SimulationResults`, and
`Controllers`. Unreadable or unsupported scenario libraries are preserved in
`SaveGames\Recovery`; routine rolling copies are in `SaveGames\Backups`.

Do not post confidential scenarios, proprietary controller source, private
paths, credentials, or other sensitive information in a public issue. PHAROS
is provided without a guaranteed response, resolution, continued maintenance,
or support level, subject to the applicable licenses, packaged EULA, and
non-waivable rights. For a community build, identify its publisher and commit
when reporting a problem.
