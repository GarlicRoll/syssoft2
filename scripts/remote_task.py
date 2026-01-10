#!/usr/bin/env python3
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ENV_FILE = ROOT / ".env"


def load_env():
    if not ENV_FILE.exists():
        print(f"Error: .env file not found in project root ({ROOT})", file=sys.stderr)
        sys.exit(1)
    for line in ENV_FILE.read_text().splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if "=" not in stripped:
            continue
        key, value = stripped.split("=", 1)
        key = key.strip()
        value = value.strip()
        if (value.startswith('"') and value.endswith('"')) or (
            value.startswith("'") and value.endswith("'")
        ):
            value = value[1:-1]
        os.environ[key] = value


def build_task_args(argv):
    if len(argv) == 0:
        return ["-a"]
    result = []
    args_iter = list(argv)
    while args_iter:
        arg = args_iter.pop(0)
        if "=" in arg:
            key, val = arg.split("=", 1)
            result.extend([key, val])
        else:
            result.append(arg)
    return result


def run_with_expect(cmd, login, password):
    script = f"""
        set timeout -1
        spawn -noecho {' '.join(shlex.quote(c) for c in cmd)}
        expect {{
            -re "Login:" {{
                send "{login}\\r"
                exp_continue
            }}
            -re "Password:" {{
                send "{password}\\r"
                exp_continue
            }}
            eof
        }}
    """
    return subprocess.run(["expect", "-c", script], check=False).returncode


def main():
    load_env()

    tool = os.environ.get("REMOTE_TASKS_TOOL")
    login = os.environ.get("REMOTE_TASKS_LOGIN")
    password = os.environ.get("REMOTE_TASKS_PASSWORD")
    extra_opts = os.environ.get("REMOTE_TASKS_EXTRA_OPTS", "")
    auto_login_env = os.environ.get("REMOTE_TASKS_AUTO_LOGIN", "1").lower()
    auto_login = auto_login_env in ("1", "true", "yes")

    if not tool:
        print("Error: REMOTE_TASKS_TOOL is not set in .env", file=sys.stderr)
        sys.exit(1)
    if not login or not password:
        print("Error: credentials missing (REMOTE_TASKS_LOGIN / REMOTE_TASKS_PASSWORD)", file=sys.stderr)
        sys.exit(1)

    task_args = build_task_args(sys.argv[1:])
    if extra_opts:
        task_args.extend(shlex.split(extra_opts))

    common_args = []
    if login and password:
        common_args.extend(["-ul", login, "-up", password])

    cmd = ["wine", tool, *common_args, *task_args]
    print("Running RemoteTasks " + " ".join(shlex.quote(c) for c in cmd))

    try:
        if auto_login and shutil.which("expect"):
            rc = run_with_expect(cmd, login, password)
        else:
            rc = subprocess.run(cmd, check=False).returncode
    except FileNotFoundError as exc:
        print(f"Error: failed to run command: {exc}", file=sys.stderr)
        sys.exit(1)

    sys.exit(rc)


if __name__ == "__main__":
    main()
