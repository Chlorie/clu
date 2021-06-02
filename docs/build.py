import subprocess
import shutil
from argparse import ArgumentParser
from pathlib import Path

if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("output", type=str, default="")
    output = Path(parser.parse_args().output).resolve()
    standardese_out = output / "api"
    current = Path(__file__).parent
    contents = current / "contents"
    includes = (current / "../lib/include").resolve()

    shutil.copytree(contents, output, dirs_exist_ok=True)
    headers = list(includes.glob("**/*.h")) + list(includes.glob("**/*.hpp"))
    ini = current / "standardese.ini"
    command = ["standardese", "-c", str(ini), "--output.prefix", str(standardese_out) + "/"]
    command += [str(path) for path in headers]
    subprocess.run(command)
