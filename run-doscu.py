
import sys
import subprocess
import tempfile
import os
import shlex
import shutil
from pathlib import Path

# Paths
ROOT = Path(__file__).resolve().parent
RUNTIME = ROOT / "runtime.cpp"


def main():
    # Get the currently selected DOSCU file
    if len(sys.argv) < 2:
        sys.exit("Error: No DOSCU file selected")

    source_file = Path(sys.argv[1]).resolve()

    if not source_file.is_file():
        sys.exit(f"Error: File not found: {source_file}")

    if source_file.suffix.lower() != ".doscu":
        sys.exit("Error: Please open a .doscu file")

    if not RUNTIME.is_file():
        sys.exit(f"Error: runtime.cpp not found: {RUNTIME}")

    try:
        compiler = find_compiler()
        with tempfile.TemporaryDirectory(prefix="doscu-") as build_dir:
            output = Path(build_dir) / ("doscu.exe" if os.name == "nt" else "doscu")
            compiler_name = Path(compiler[0]).name.lower()
            if compiler_name in {"cl", "cl.exe"}:
                build_command = [
                    *compiler, "/nologo", "/std:c++17", "/EHsc",
                    str(RUNTIME), f"/Fe:{output}",
                    f"/Fo:{Path(build_dir) / 'runtime.obj'}",
                    f"/Fd:{Path(build_dir) / 'runtime.pdb'}"
                ]
            else:
                build_command = [
                    *compiler, "-std=c++17", str(RUNTIME), "-o", str(output)
                ]

            subprocess.run(build_command, cwd=ROOT, check=True)

            # The runtime accepts a source path, so multiple .doscu files can
            # be run without rewriting runtime.cpp or depending on its cwd.
            subprocess.run([str(output), str(source_file)], cwd=ROOT, check=True)

    except subprocess.CalledProcessError as error:
        sys.exit(f"Error: Build or runtime failed: {error}")

    except FileNotFoundError:
        sys.exit(
            "Error: the configured C++ compiler could not be started. "
            "Install clang++ or g++, or set CXX to the compiler command."
        )


def find_compiler():
    configured = os.environ.get("CXX")
    candidates = [shlex.split(configured)] if configured else []
    candidates.extend([[name] for name in ("clang++", "g++", "c++", "cl")])

    for candidate in candidates:
        if not candidate:
            continue
        executable = candidate[0]
        if shutil.which(executable) or Path(executable).is_file():
            return candidate

    raise FileNotFoundError(
        "No C++17 compiler found. Install clang++ or g++, or set CXX."
    )


if __name__ == "__main__":
    main()
