import subprocess
import shutil
from argparse import ArgumentParser
from pathlib import Path


def escape_code_block(path: Path):
    lines = []
    with open(path, "r") as file:
        cpp = False
        for line in file:
            if line == "``` cpp\n":
                cpp = True
                lines.append("{% raw %}\n")
            lines.append(line)
            if line == "```\n" and cpp:
                cpp = False
                lines.append("{% endraw %}\n")
    with open(path, "w") as file:
        file.writelines(lines)


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
    for path in standardese_out.glob("*.md"):
        escape_code_block(path)
