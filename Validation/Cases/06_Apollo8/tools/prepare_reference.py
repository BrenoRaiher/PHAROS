"""Regenerate the complete historical-state reference and saved-output comparisons.

The frozen direct_states.json preserves the inputs actually used for calibration.
It is not the corrected catalog used for the final historical comparisons.
Rounded Table 5-II conversions are already archived with their source fields.
"""
from pathlib import Path
import runpy
if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).with_name('expand_historical_reference.py')), run_name='__main__')
