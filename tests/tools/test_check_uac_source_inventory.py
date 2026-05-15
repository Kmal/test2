import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]

def test_uac_source_inventory_check_passes():
    subprocess.run([str(ROOT / "tools" / "check_uac_source_inventory.py")], cwd=ROOT, check=True)
