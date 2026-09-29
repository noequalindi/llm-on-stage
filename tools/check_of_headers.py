"""Syntax-check the lesson addon and sketch with an installed macOS OF SDK."""
import subprocess
import sys
from pathlib import Path

sdk = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[1]
includes = [sdk / "libs/openFrameworks", root / "ofxLocalLLM/src", root / "llmPoster/src"]
includes.extend(p for p in (sdk / "libs/openFrameworks").iterdir() if p.is_dir())
for p in (sdk / "libs").glob("*/include"):
    includes.append(p)
    includes.extend(child for child in p.iterdir() if child.is_dir())
command = ["clang++", "-std=c++17", "-fsyntax-only", "-DGL_SILENCE_DEPRECATION", "-Wno-deprecated-declarations"]
for p in includes:
    command.extend(["-I", str(p)])
sources = list((root / "ofxLocalLLM/src").glob("*.cpp")) + list((root / "llmPoster/src").glob("*.cpp"))
if "--stage" in sys.argv[2:]:
    box = sdk / "addons/ofxBox2d"
    for p in [box / "src", box / "libs", box / "libs/Box2D"]:
        command.extend(["-I", str(p)])
    sources += list((root / "llmStage/src").glob("*.cpp"))
subprocess.run(command + [str(p) for p in sources], check=True)
print("Direct Ollama addon and selected sketches: syntax OK")
