#!/usr/bin/env python3

import argparse
import json
import zipfile
from pathlib import Path, PurePosixPath

import docker
from docker.models.containers import Container

parser = argparse.ArgumentParser(
    description="Packages all alternative entry points of a binary compiled with the cppless alt-entry option"
)
parser.add_argument(
    "input", metavar="input", type=str, help="The input file to process."
)
parser.add_argument(
    "-s",
    "--sysroot",
    metavar="sysroot",
    type=str,
    help="The sysroot to use.",
    required=True,
)
parser.add_argument(
    "-p",
    "--project",
    metavar="project",
    type=str,
    help="The root directory of the project.",
    required=True,
)
parser.add_argument(
    "-i",
    "--image",
    metavar="image",
    type=str,
    help="The docker image to use.",
    required=True,
)
parser.add_argument(
    "-d",
    "--libc",
    metavar="libc",
    type=bool,
    help="Do not use libc.",
    default=False,
    action=argparse.BooleanOptionalAction,
)

args = parser.parse_args()

input_path = Path(args.input)
sysroot_path = Path(args.sysroot)
project_path = Path(args.project).absolute()
image = args.image
libc = args.libc


container_root = PurePosixPath("/usr/src/project/")

docker_client = docker.from_env()


class ContainerWrapper:
    container: Container

    def __init__(self, container):
        self.container = container

    def __enter__(self):
        return self.container

    def __exit__(self, exc_type, exc_value, tb):
        self.container.stop(timeout=0)
        self.container.remove()


boostrap_libc_script_template = """
#!/bin/bash
set -euo pipefail
export AWS_EXECUTION_ENV=lambda-cpp
exec $LAMBDA_TASK_ROOT/lib/{pkg_ld} --library-path $LAMBDA_TASK_ROOT/lib $LAMBDA_TASK_ROOT/bin/{pkg_bin_filename} ${{_HANDLER}}
""".strip()


def generate_libc_bootstrap_script(
    pkg_ld: str,
    pkg_bin_filename: str,
):
    return boostrap_libc_script_template.format(
        pkg_ld=pkg_ld, pkg_bin_filename=pkg_bin_filename
    )


bootstrap_no_libc_script_template = """
#!/bin/bash
set -euo pipefail
export AWS_EXECUTION_ENV=lambda-cpp
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LAMBDA_TASK_ROOT/lib
exec $LAMBDA_TASK_ROOT/bin/{pkg_bin_filename} ${{_HANDLER}}
""".strip()


def generate_no_libc_bootstrap_script(
    pkg_bin_filename: str,
):
    return bootstrap_no_libc_script_template.format(pkg_bin_filename=pkg_bin_filename)


def aws_lambda_package(
    executable_path: Path,
    sysroot_path: Path,
    project_path: Path,
    container: Container,
    container_root: PurePosixPath,
    libc: bool,
):
    def local_path_to_container_path(local_path: Path) -> PurePosixPath:
        return PurePosixPath(container_root / local_path.relative_to(project_path))

    zip_path = executable_path.with_suffix(".zip")
    with zipfile.ZipFile(str(zip_path), "w") as zf:
        bin = PurePosixPath("bin")
        lib = PurePosixPath("lib")
        _, ldd_output = container.exec_run(
            [
                "ldd",
                local_path_to_container_path(executable_path).as_posix(),
            ]
        )
        lib_paths: set[PurePosixPath] = set()

        ldd_lines = ldd_output.decode("utf-8").split("\n")
        for line in ldd_lines:
            stripped = line.strip()
            parts = stripped.split(" ")
            if len(parts) == 2:
                lib_path = PurePosixPath(parts[0])
                lib_paths.add(lib_path)
            elif len(parts) == 4:
                lib_path = PurePosixPath(parts[2])
                lib_paths.add(lib_path)

        _, apk_output = container.exec_run(["apk", "info", "--contents", "musl"])
        apk_lines = apk_output.decode("utf-8").split("\n")[1:]
        for line in apk_lines:
            stripped = line.strip()
            if not stripped:
                continue
            lib_paths.add(PurePosixPath("/" + stripped))

        for lib_path in lib_paths:
            local_path = sysroot_path / lib_path.relative_to(lib_path.anchor)
            zf.write(str(local_path), arcname=(lib / local_path.name).as_posix())

        zf.write(executable_path, arcname=(bin / executable_path.name).as_posix())
        pkg_bin_filename = executable_path.name
        if libc:
            pkg_ld_filter = filter(lambda p: p.name.startswith("ld-"), lib_paths)
            if len(pkg_ld_filter) != 1:
                raise Exception(
                    "Expected exactly one ld-* library, found {}".format(
                        len(pkg_ld_filter)
                    )
                )
            pkg_ld = next(pkg_ld_filter)
            zf.writestr(
                "bootstrap", generate_libc_bootstrap_script(pkg_ld, pkg_bin_filename)
            )
        else:
            zf.writestr(
                "bootstrap", generate_no_libc_bootstrap_script(pkg_bin_filename)
            )


container = docker_client.containers.run(
    image,
    ["tail", "-f", "/dev/null"],
    detach=True,
    volumes={str(project_path): {"bind": container_root.as_posix(), "mode": "rw"}},
)


json_path = input_path.with_suffix(".json")

with json_path.open("r") as f:
    data = json.load(f)

entry_points = data["entry_points"]
for entry_point in entry_points:
    entry_file_name = entry_point["filename"]
    entry_file_path = input_path.parent / entry_file_name
    with ContainerWrapper(container) as container:
        aws_lambda_package(
            entry_file_path, sysroot_path, project_path, container, container_root, libc
        )
