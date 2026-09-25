# PHAROS 1.0 Privacy and Local Data

PHAROS 1.0 contains no PHAROS analytics service, telemetry uploader, account
system, or automatic crash-report submission. Scenario libraries, controller
workspaces, simulation results, settings, and diagnostic logs are stored
locally on the user's computer.

On packaged Windows installations, PHAROS data is normally stored under:

`%LOCALAPPDATA%\PHAROS\Saved`

PHAROS does not transmit those files to the project author. Data leaves the
computer only through an action taken by the user or by software the user
chooses to run. Examples include opening the PHAROS GitHub link in a browser,
manually sharing a diagnostic file, or executing a custom controller that
performs its own external communication.

Custom C++ controllers execute as native code with the permissions available
to PHAROS. Review and trust controller code before compiling or loading it.

External websites opened from PHAROS are governed by their own privacy terms.
This notice describes PHAROS version 1.0 and should be reviewed again if a
later release adds networking, telemetry, accounts, or automatic reporting.
