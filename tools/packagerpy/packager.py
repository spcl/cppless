#!/usr/bin/env python3
import argparse
import base64
from datetime import datetime
import hashlib
import io
import json
import os
import time
import zipfile
from pathlib import Path, PurePosixPath
from threading import Thread
from typing import List
from encoding import decode_value

import boto3
import botocore.exceptions
import docker
from docker.models.containers import Container
from mypy_boto3_lambda import LambdaClient


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
    "--libc",
    metavar="libc",
    type=bool,
    help="Do not use libc.",
    default=False,
    action=argparse.BooleanOptionalAction,
)
parser.add_argument(
    "--strip",
    metavar="strip",
    type=bool,
    help="Strip the executable.",
    default=False,
    action=argparse.BooleanOptionalAction,
)
parser.add_argument(
    "--deploy",
    metavar="deploy",
    type=bool,
    help="Deploy the package to AWS Lambda.",
    default=False,
    action=argparse.BooleanOptionalAction,
)
parser.add_argument(
    "-r",
    "--function-role-arn",
    metavar="function-role-arn",
    type=str,
    help="The role ARN to use for the Lambda functions.",
    required=False,
    default="",
)
parser.add_argument(
    "-t",
    "--target-name",
    metavar="target-name",
    type=str,
    help="The target name to use.",
    required=True,
)

some_time = datetime.utcfromtimestamp(420000000)

args = parser.parse_args()

input_path = Path(args.input)
sysroot_path = Path(args.sysroot)
project_path = Path(args.project).absolute()
image = args.image
libc = args.libc
strip = args.strip
deploy = args.deploy
function_role_arn = args.function_role_arn
target_name = args.target_name

container_root = PurePosixPath("/usr/src/project/")
aws_lambda: LambdaClient = boto3.client("lambda")
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


def get_zinfo(zf: zipfile.ZipFile, name: str, external_attr: int) -> zipfile.ZipInfo:
    zinfo = zipfile.ZipInfo(filename=name, date_time=time.gmtime(420000000)[:6])
    zinfo.compress_type = zf.compression
    zinfo._compresslevel = zf.compresslevel
    zinfo.external_attr = external_attr
    return zinfo


def strip_binary(
    executable_path: Path,
    project_path: Path,
    container: Container,
    container_root: PurePosixPath,
):
    def local_path_to_container_path(local_path: Path) -> PurePosixPath:
        return PurePosixPath(container_root / local_path.relative_to(project_path))

    stripped_executable_path = executable_path.with_name(
        executable_path.name + ".stripped"
    )
    # Create copy of file
    with executable_path.open("rb") as f:
        with stripped_executable_path.open("wb") as g:
            g.write(f.read())
    _, _ = container.exec_run(
        ["strip", local_path_to_container_path(stripped_executable_path).as_posix()]
    )


def get_musl_paths(container: Container):
    paths: set[PurePosixPath] = set()

    _, apk_output = container.exec_run(["apk", "info", "--contents", "musl"])
    apk_lines = apk_output.decode("utf-8").split("\n")[1:]
    for line in apk_lines:
        stripped = line.strip()
        if not stripped:
            continue
        paths.add(PurePosixPath("/" + stripped))
    return paths


def aws_lambda_package(
    executable_path: Path,
    sysroot_path: Path,
    project_path: Path,
    container: Container,
    container_root: PurePosixPath,
    libc: bool,
    musl_paths: set[PurePosixPath],
):
    def local_path_to_container_path(local_path: Path) -> PurePosixPath:
        return PurePosixPath(container_root / local_path.relative_to(project_path))

    zip = io.BytesIO()
    with zipfile.ZipFile(zip, "a") as zf:
        bin = PurePosixPath("bin")
        lib = PurePosixPath("lib")
        _, ldd_output = container.exec_run(
            [
                "ldd",
                local_path_to_container_path(executable_path).as_posix(),
            ]
        )
        lib_paths: set[PurePosixPath] = set()

        for path in musl_paths:
            lib_paths.add(path)

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

        for lib_path in lib_paths:
            local_path = sysroot_path / lib_path.relative_to(lib_path.anchor)
            if local_path.name.startswith("ld-"):
                lib_zinfo = get_zinfo(
                    zf, (lib / local_path.name).as_posix(), 0o755 << 16
                )
            else:
                lib_zinfo = get_zinfo(
                    zf, (lib / local_path.name).as_posix(), 0o644 << 16
                )
            zf.writestr(lib_zinfo, local_path.read_bytes())

        exec_zinfo = get_zinfo(zf, (bin / executable_path.name).as_posix(), 0o755 << 16)
        zf.writestr(exec_zinfo, executable_path.read_bytes())
        pkg_bin_filename = executable_path.name

        if libc:
            pkg_ld_filter = list(filter(lambda p: p.name.startswith("ld-"), lib_paths))
            if len(pkg_ld_filter) != 1:
                raise Exception(
                    "Expected exactly one ld-* library, found {}".format(
                        len(pkg_ld_filter)
                    )
                )
            pkg_ld = pkg_ld_filter[0]

            zf.writestr(
                get_zinfo(zf, "bootstrap", 0o755 << 16),  # ?rwxrwxrwx
                generate_libc_bootstrap_script(pkg_ld.name, pkg_bin_filename),
            )
        else:
            zf.writestr(
                get_zinfo(zf, "bootstrap", 0o755 << 16),  # ?rwxrwxrwx
                generate_no_libc_bootstrap_script(pkg_bin_filename),
            )
    return zip


def human_readable_size(size, decimal_places=2):
    for unit in ["B", "KiB", "MiB", "GiB", "TiB", "PiB"]:
        if size < 1024.0 or unit == "PiB":
            break
        size /= 1024.0
    return f"{size:.{decimal_places}f} {unit}"


def handle_entry_point(
    entry_file_path: Path,
    user_meta: dict,
    strip: bool,
    deploy: bool,
    function_role_arn: str,
    musl_paths: set[PurePosixPath],
):
    if strip:
        strip_binary(entry_file_path, project_path, container, container_root)
    zip = aws_lambda_package(
        entry_file_path,
        sysroot_path,
        project_path,
        container,
        container_root,
        libc,
        musl_paths,
    )
    zip_bytes = zip.getvalue()
    zip.close()

    zip_sha256 = hashlib.sha256(zip_bytes).digest()
    zip_sha256_base64 = base64.b64encode(zip_sha256).decode("utf-8")
    if deploy:
        identifier: str = user_meta["identifier"]
        ephemeral_storage: int = user_meta["ephemeral_storage"]
        memory: int = user_meta["memory"]
        timeout: int = user_meta["timeout"]

        hash = hashlib.sha256()
        hash.update(identifier.encode("utf-8"))
        hash.update("#".encode("utf-8"))
        hash.update(str(ephemeral_storage).encode("utf-8"))
        hash.update("#".encode("utf-8"))
        hash.update(str(memory).encode("utf-8"))
        hash.update("#".encode("utf-8"))
        hash.update(str(timeout).encode("utf-8"))

        # hash & hex encode the function name to avoid issues with special characters
        function_name = target_name + "-" + hash.digest().hex()[:8]

        print(function_name)

        fn = None
        try:
            fn = aws_lambda.get_function(FunctionName=function_name)
        except botocore.exceptions.ClientError:
            pass

        actions: List[str] = []
        if fn is not None:
            if fn["Configuration"]["CodeSha256"] != zip_sha256_base64:
                actions.append("update-code")
            if fn["Configuration"]["Role"] != function_role_arn:
                actions.append("update-role")

            if fn["Configuration"]["Timeout"] != timeout:
                actions.append("update-config")
            elif fn["Configuration"]["MemorySize"] != memory:
                actions.append("update-config")
            # elif fn["Configuration"]["EphemeralStorage"]["Size"] != ephemeral_storage:
            #    actions.append("update-config")

        else:
            actions.append("create")

        print(
            "{provided_function_name}\n  Name: {function_name}\n  Sha256: {zip_sha256_base64}\n  Actions: {action}\n  Size: {size}".format(
                provided_function_name=identifier,
                function_name=function_name,
                zip_sha256_base64=zip_sha256_base64,
                action=", ".join(actions) if actions else "None",
                size=human_readable_size(len(zip_bytes)),
            )
        )

        if "create" in actions:
            aws_lambda.create_function(
                FunctionName=function_name,
                Runtime="provided",
                Role=function_role_arn,
                Handler="bootstrap",
                Code={"ZipFile": zip_bytes},
                Timeout=timeout,
                MemorySize=memory,
                # EphemeralStorage={"Size": ephemeral_storage} aws_lambda.,
            )
        if "update-config" in actions:
            aws_lambda.update_function_configuration(
                FunctionName=function_name,
                Timeout=timeout,
                MemorySize=memory,
                # EphemeralStorage={"Size": ephemeral_storage},
            )
        if "update-code" in actions:
            aws_lambda.update_function_code(
                FunctionName=function_name, ZipFile=zip_bytes
            )
        if "update-role" in actions:
            aws_lambda.update_function_configuration(
                FunctionName=function_name,
                Role=function_role_arn,
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
with ContainerWrapper(container) as container:
    musl_paths = get_musl_paths(container)

    threads: List[Thread] = []
    print("Deploying {} lambda functions".format(len(entry_points)))
    for entry_point in entry_points:
        entry_file_name = entry_point["filename"]
        user_meta_encoded = entry_point["user_meta"]

        user_meta_binary = io.BytesIO(base64.b64decode(user_meta_encoded))
        user_meta = decode_value(user_meta_binary)

        print(user_meta)

        entry_file_path = input_path.parent / entry_file_name

        thread = Thread(
            target=handle_entry_point,
            args=(
                entry_file_path,
                user_meta,
                strip,
                deploy,
                function_role_arn,
                musl_paths,
            ),
        )
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()
