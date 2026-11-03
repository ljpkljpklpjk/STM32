"""Compile and run the real control/filter C code with mocked hardware.

Usage: python tests/run_tests.py --cc gcc
       python tests/run_tests.py --cc C:/path/to/zig.exe --zig
Requires Python 3 and a host C compiler. Builds only inside a temporary folder.
"""
import argparse
import os
import pathlib
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--zig", action="store_true")
    args = parser.parse_args()
    project = pathlib.Path(__file__).resolve().parent.parent
    user = project / "USER"
    xml = ET.parse(user / "TempHumi.uvprojx")
    includes = xml.find(".//Cads/VariousControls/IncludePath").text.split(";")
    paths = [(user / p.replace("\\", "/")).resolve() for p in includes]
    for entry in xml.findall(".//File"):
        source = (user / entry.findtext("FilePath").replace("\\", "/")).resolve()
        if not source.is_file():
            raise RuntimeError(f"Missing Keil project source: {source}")
    assert sum(e.findtext("FileName") == "fuzzy_pid.c" for e in xml.findall(".//File")) == 1
    command = [args.cc] + (["cc"] if args.zig else [])
    flags = ["-std=gnu90", "-Wall", "-Wextra", "-Werror", "-Wno-invalid-utf8",
             "-include", str(project / "tests/platform_stub.h")]
    for path in paths:
        flags.extend(["-I", str(path)])
    with tempfile.TemporaryDirectory(prefix="stm32-control-test-") as temp:
        env = os.environ.copy()
        env["ZIG_LOCAL_CACHE_DIR"] = str(pathlib.Path(temp) / "local-cache")
        env["ZIG_GLOBAL_CACHE_DIR"] = str(pathlib.Path(temp) / "global-cache")
        exe = pathlib.Path(temp) / "control_tests.exe"
        build = command + flags + [str(project / "tests/test_control.c"),
                                   str(project / "HARDWARE/CONTROL/fuzzy_pid.c"),
                                   "-lm", "-o", str(exe)]
        subprocess.run(build, check=True, cwd=temp, env=env)
        subprocess.run([str(exe)], check=True, cwd=temp, env=env)
        # Verify changed application callers against actual project headers.
        for source in [user / "main.c", project / "HARDWARE/DISPLAY/display.c"]:
            syntax = command + flags + ["-c", str(source), "-o",
                                        str(pathlib.Path(temp) / (source.stem + ".o"))]
            subprocess.run(syntax, check=True, cwd=temp, env=env)
    print("PASS Keil source registration and application interface syntax")


if __name__ == "__main__":
    main()
