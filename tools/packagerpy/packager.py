#!/usr/bin/env python3
import argparse
import base64
import hashlib
import io
import json
import os
import re
import subprocess
import time
import zipfile
from datetime import datetime
from pathlib import Path, PurePosixPath
from threading import Thread
from typing import List, Set, Optional

import boto3
import botocore.exceptions
import docker
from docker.models.containers import Container
from mypy_boto3_lambda import LambdaClient

from encoding import decode_value

parser = argparse.ArgumentParser(
    description="Packages all alternative entry points of a binary compiled with the cppless alt-entry option"
)
parser.add_argument(
    "input", metavar="input", type=str, help="The input file to process."
)
parser.add_argument(
    "-s", "--sysroot", type=str, help="The sysroot to use.", default="", required=False
)
parser.add_argument(
    "--architecture",
    type=str,
    help="Architecture of target",
    default="x64",
    required=False,
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
    "--project-source",
    metavar="project_source",
    default="",
    type=str,
    help="The root source directory of the project.",
    required=False,
)
parser.add_argument(
    "-i",
    "--image",
    type=str,
    help="The docker image to use.",
    default="",
    required=False,
)
parser.add_argument(
    "--libc", help="Do not use libc.", default=False, action="store_true"
)
parser.add_argument(
    "--strip", help="Strip the executable.", default=False, action="store_true"
)
parser.add_argument(
    "--deploy",
    help="Deploy the package to AWS Lambda.",
    default=False,
    action="store_true",
)
parser.add_argument(
    "-r",
    "--function-role-arn",
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
if args.sysroot:
    sysroot_path = Path(args.sysroot)
else:
    sysroot_path = None
project_path = Path(args.project).absolute()
project_source = Path(args.project_source).absolute()
libc = args.libc
strip = args.strip
deploy = args.deploy
function_role_arn = args.function_role_arn
target_name = args.target_name
architecture = args.architecture

region = os.environ.get("AWS_REGION", "")
access_key_id = os.environ.get("AWS_ACCESS_KEY_ID", "")
secret_access_key = os.environ.get("AWS_SECRET_ACCESS_KEY", "")
function_role_arn = (
    os.environ.get("AWS_FUNCTION_ROLE_ARN", "")
    if function_role_arn == ""
    else function_role_arn
)

if (
    region == ""
    or access_key_id == ""
    or secret_access_key == ""
    or function_role_arn == ""
):
    config_path = Path("~/.cppless/config.json").expanduser()
    if config_path.exists():
        with config_path.open("r") as f:
            config = json.load(f)
        profile_name = "default"
        for profile in config["profiles"]:
            if profile["name"] == profile_name:
                active_profile = profile["config"]
                break
        aws_config = active_profile["providers"]["aws"]
        if region == "":
            region = aws_config["region"]
        if access_key_id == "":
            access_key_id = aws_config["access_key_id"]
        if secret_access_key == "":
            secret_access_key = aws_config["secret_access_key"]
        if function_role_arn == "":
            function_role_arn = aws_config["function_role_arn"]

session = boto3.session.Session(access_key_id, secret_access_key, None, region)
aws_lambda: LambdaClient = session.client("lambda")
s3_client = session.client('s3', region_name=region)

image = args.image


def parse_ldd(ldd_output: str):
    lib_paths = set()

    ldd_lines = ldd_output.decode("utf-8").split("\n")
    for line in ldd_lines:
        stripped = line.strip()
        parts = stripped.split(" ")
        if len(parts) == 2:
            if not parts[0].startswith("/"):
                continue
            lib_path = PurePosixPath(parts[0])
            lib_paths.add(lib_path)
        elif len(parts) == 4:
            lib_path = PurePosixPath(parts[2])
            lib_paths.add(lib_path)

    return lib_paths


class NativeEnvironment:
    def __init__(self):
        pass

    def start(self):
        pass

    def stop(self):
        pass

    def strip(self, path):
        _ = subprocess.run(["strip", path])

    def ldd(self, path):
        ldd = subprocess.run(["ldd", path], capture_output=True)
        ldd_output = ldd.stdout
        paths = parse_ldd(ldd_output)
        return paths

    def get_libc_paths(self):
        paths: Set[PurePosixPath] = set()
        rpm = subprocess.run(
            ["dpkg-query", "-L", "libc6"], capture_output=True
            #["rpm", "--query", "--list", "glibc.x86_64"], capture_output=True
        )
        rpm_output = rpm.stdout
        for file in rpm_output.decode("utf-8").split("\n"):
            if not (re.match(r"^.+\.so(\.[0-9]+)?$", file)) or not Path(file).is_file():
                continue
            paths.add(PurePosixPath(file))
        return paths


class CrossArmEnvironment:
    def __init__(self):
        pass

    def start(self):
        pass

    def stop(self):
        pass

    def strip(self, path):
        pass

    def ldd(self, path):
        args = []
        args.append("/bin/bash")
        args.append(project_source / "tools" / "packagerpy" / "cross-compile-ldd" / "cross-compile-ldd")
        args.append("--root")
        args.append(sysroot_path)
        args.append(path)
        ldd = subprocess.run(args, capture_output=True)
        ldd_output = ldd.stdout
        paths = parse_ldd(ldd_output)
        print(ldd_output.decode("utf-8"))
        return paths

    def get_libc_paths(self):
        paths = set([PurePosixPath("/lib/ld-linux-aarch64.so.1")])
        return paths


class DockerEnvironment:
    container: Container
    container_root = PurePosixPath("/usr/src/project/")

    def __init__(self, image):
        container = docker_client.containers.run(
            image,
            ["tail", "-f", "/dev/null"],
            detach=True,
            volumes={
                str(project_path): {
                    "bind": self.container_root.as_posix(),
                    "mode": "rw",
                }
            },
        )
        self.container = container

    def start(self):
        return self.container

    def stop(self):
        self.container.stop(timeout=0)
        self.container.remove()

    def strip(self, path):
        _, _ = self.container.exec_run(
            ["strip", self.local_path_to_container_path(path).as_posix()]
        )

    def ldd(self, path):
        _, ldd_output = self.container.exec_run(
            [
                "ldd",
                self.local_path_to_container_path(path).as_posix(),
            ]
        )

        ldd_paths = parse_ldd(ldd_output)
        return ldd_paths

    def local_path_to_container_path(self, local_path: Path) -> PurePosixPath:
        return PurePosixPath(self.container_root / local_path.relative_to(project_path))

    def get_libc_paths(self):
        paths: Set[PurePosixPath] = set()

        _, apk_output = self.container.exec_run(["apk", "info", "--contents", "musl"])
        apk_lines = apk_output.decode("utf-8").split("\n")[1:]
        for line in apk_lines:
            stripped = line.strip()
            if not stripped:
                continue
            paths.add(PurePosixPath("/" + stripped))
        return paths


class EnvironmentWrapper:
    def __init__(self, environment):
        self.environment = environment

    def __enter__(self):
        return self.environment.start()

    def __exit__(self, exc_type, exc_value, tb):
        self.environment.stop()


if image:
    docker_client = docker.from_env()
    environment = DockerEnvironment(docker_client, image)
elif architecture == "aarch64":
    environment = CrossArmEnvironment()
else:
    environment = NativeEnvironment()


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


def strip_binary(executable_path: Path, environment):
    stripped_executable_path = executable_path.with_name(
        executable_path.name + ".stripped"
    )
    # Create copy of file
    with executable_path.open("rb") as f:
        with stripped_executable_path.open("wb") as g:
            g.write(f.read())
    environment.strip(stripped_executable_path)
    return stripped_executable_path


def aws_lambda_package(
    executable_path: Path,
    sysroot_path: Optional[Path],
    environment,
    libc: bool,
    libc_paths: Set[PurePosixPath],
):
    zip = io.BytesIO()
    with zipfile.ZipFile(
        zip, "a", compression=zipfile.ZIP_BZIP2, compresslevel=9
    ) as zf:
        bin = PurePosixPath("bin")
        lib = PurePosixPath("lib")

        lib_paths: Set[PurePosixPath] = set()

        lib_paths = environment.ldd(executable_path)

        def add_path(lib_path):
            if sysroot_path:
                local_path = sysroot_path / lib_path.relative_to(lib_path.anchor)
            else:
                local_path = Path(lib_path)
            if local_path.name.startswith("ld-"):
                lib_zinfo = get_zinfo(
                    zf, (lib / local_path.name).as_posix(), 0o755 << 16
                )
            else:
                lib_zinfo = get_zinfo(
                    zf, (lib / local_path.name).as_posix(), 0o644 << 16
                )
            zf.writestr(lib_zinfo, local_path.read_bytes())

        for lib_path in lib_paths:
            add_path(lib_path)
        if libc:
            for lib_path in libc_paths:
                if lib_path in lib_paths:
                    continue
                add_path(lib_path)

        exec_zinfo = get_zinfo(zf, (bin / executable_path.name).as_posix(), 0o755 << 16)
        zf.writestr(exec_zinfo, executable_path.read_bytes())
        pkg_bin_filename = executable_path.name

        pkg_ld_filter = list(
            filter(lambda p: p.name.startswith("ld-") and p in lib_paths, lib_paths)
        )
        if len(pkg_ld_filter) != 1:
            raise Exception(
                "Expected exactly one ld-* library, found {}: {}".format(
                    len(pkg_ld_filter), ", ".join(map(str, pkg_ld_filter))
                )
            )
        pkg_ld = pkg_ld_filter[0]

        if libc:
            zf.writestr(
                get_zinfo(zf, "bootstrap", 0o755 << 16),  # ?rwxrwxrwx
                generate_libc_bootstrap_script(pkg_ld.name, pkg_bin_filename),
            )
        else:
            zf.writestr(
                get_zinfo(zf, "bootstrap", 0o755 << 16),  # ?rwxrwxrwx
                generate_no_libc_bootstrap_script(pkg_bin_filename),
            )
    with open("output.zip", "wb") as f:
        f.write(zip.getbuffer())
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
    environment,
    architecture: str,
    musl_paths: Set[PurePosixPath],
):
    executable_path = entry_file_path
    if strip:
        executable_path = strip_binary(entry_file_path, environment)
    zip = aws_lambda_package(
        executable_path,
        sysroot_path,
        environment,
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

        print(f"Deploying {function_name}")

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

        s3_bucket_name = "cppless-code"
        if len(zip_bytes) > 70160000:

            s3_key = f"lambda-code/{function_name}-{zip_sha256_base64}.zip"

            # Upload code to S3
            print(f"Code size exceeds threshold. Uploading to S3: {s3_bucket_name}/{s3_key}")
            s3_client.put_object(
                Bucket=s3_bucket_name,
                Key=s3_key,
                Body=zip_bytes
            )

            # Modified code reference using S3 instead of direct ZipFile
            code_reference = {
                "S3Bucket": s3_bucket_name,
                "S3Key": s3_key
            }
        else:
            code_reference = {"ZipFile": zip_bytes}

        try:
            if "create" in actions:
                if architecture == "x64":
                    architectures = ["x86_64"]
                elif architecture == "aarch64":
                    architectures = ["arm64"]
                else:
                    raise NotImplementedError("Unsupported architecture")
                print("Architecture", architectures)
                aws_lambda.create_function(
                    FunctionName=function_name,
                    Runtime="provided.al2023",
                    Role=function_role_arn,
                    Handler="bootstrap",
                    Code=code_reference,
                    Timeout=timeout,
                    MemorySize=memory,
                    Architectures=architectures,
                    # EphemeralStorage={"Size": ephemeral_storage} aws_lambda.,
                )

                waiter = aws_lambda.get_waiter('function_exists')
                waiter.wait(FunctionName=function_name)
            if "update-config" in actions:
                aws_lambda.update_function_configuration(
                    FunctionName=function_name,
                    Timeout=timeout,
                    MemorySize=memory,
                    # EphemeralStorage={"Size": ephemeral_storage},
                )
                waiter = aws_lambda.get_waiter('function_updated')
                waiter.wait(FunctionName=function_name)
            if "update-code" in actions:
                aws_lambda.update_function_code(
                    FunctionName=function_name,
                    **code_reference
                )
                waiter = aws_lambda.get_waiter('function_updated')
                waiter.wait(FunctionName=function_name)
            if "update-role" in actions:
                aws_lambda.update_function_configuration(
                    FunctionName=function_name,
                    Role=function_role_arn,
                )
                waiter = aws_lambda.get_waiter('function_updated')
                waiter.wait(FunctionName=function_name)
        except Exception as e:
            print(e)


json_path = input_path.with_suffix(".json")

with json_path.open("r") as f:
    data = json.load(f)


entry_points = data["entry_points"]
with EnvironmentWrapper(environment) as e:
    libc_paths = environment.get_libc_paths()

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
                environment,
                architecture,
                libc_paths,
            ),
        )
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()
