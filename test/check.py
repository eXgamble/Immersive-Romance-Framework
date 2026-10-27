"""Immersive Romance Framework: the fast checks (seconds, no game).

    python test/check.py

- every add-on spec still builds, and the manifest it builds is the one the add-on ships (an add-on whose spec
  changed without a rebuild fails here: its lines and the manifest would no longer agree)
- the SKSE code passes the skse-dev-rules lint (when the checker is installed)

Run it after changing the generator, a spec or the C++ (build.bat runs the lint itself). The in-game tests are
test/smoke/smoke.py.
"""
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CANDIDATES = "SKSE/Plugins/ImmersiveRomanceFramework/Candidates"
# spec -> (the add-on folder, its manifest's file name, the plugin the build goes into)
ADDONS = [
    ("addons/camilla.json", "addons/Immersive Romance - Camilla", "Camilla Valerius.json", "IRCamillaBuild.esp"),
    ("test/specs/camilla.json", "test/IRF - Camilla (test)", "Camilla Valerius.json", None),
    ("test/specs/faendal.json", "test/IRF - Faendal (test)", "Faendal.json", None),
]
LINT = os.path.join(os.path.expanduser("~"), ".claude", "skills", "skse-dev-rules", "scripts", "skse_lint.py")


def diff(a, b, path=""):
    """the first place two JSON values differ"""
    if type(a) is not type(b):
        return f"{path or '/'}: {json.dumps(a)[:80]} vs {json.dumps(b)[:80]}"
    if isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a or k not in b:
                return f"{path}/{k}: only in {'the build' if k in a else 'the add-on'}"
            d = diff(a[k], b[k], f"{path}/{k}")
            if d:
                return d
    elif isinstance(a, list):
        if len(a) != len(b):
            return f"{path}: {len(a)} entries vs {len(b)}"
        for i, (x, y) in enumerate(zip(a, b)):
            d = diff(x, y, f"{path}[{i}]")
            if d:
                return d
    elif a != b:
        return f"{path}: {json.dumps(a)[:80]} vs {json.dumps(b)[:80]}"
    return None


def main():
    failed = 0
    with tempfile.TemporaryDirectory() as tmp:
        for i, (spec, folder, manifest, into) in enumerate(ADDONS):
            out = os.path.join(tmp, str(i))
            cmd = [sys.executable, "-I", os.path.join(ROOT, "tools", "irf_addon.py"), os.path.join(ROOT, spec), out]
            if into:
                cmd += ["--into", into]
            run = subprocess.run(cmd, capture_output=True, text=True)
            if run.returncode != 0:
                print(f"FAIL  {spec}: doesn't build\n{run.stderr.strip()[-800:]}")
                failed += 1
                continue
            with open(os.path.join(out, "manifest.json"), encoding="utf-8") as f:
                built = json.load(f)
            with open(os.path.join(ROOT, folder, CANDIDATES, manifest), encoding="utf-8") as f:
                shipped = json.load(f)
            d = diff(built, shipped)
            if d:
                print(f"FAIL  {spec}: the add-on's manifest isn't what the spec builds (rebuild it) - {d}")
                failed += 1
            else:
                print(f"ok    {spec}")
    if os.path.exists(LINT):
        run = subprocess.run([sys.executable, LINT, os.path.join(ROOT, "skse", "src")], capture_output=True, text=True)
        last = (run.stdout.strip().splitlines() or ["(no output)"])[-1]
        print(("ok    " if run.returncode == 0 else "FAIL  ") + last)
        failed += run.returncode != 0
    else:
        print("skip  lint (skse-dev-rules not installed)")
    print("all fine" if not failed else f"{failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
