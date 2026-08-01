# Copyright (C) 2026 CantorAI Inc.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Apply the CantorAI Apache-2.0 header to commentable repository files."""

from __future__ import annotations

import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COPYRIGHT = "Copyright (C) 2026 CantorAI Inc."
LEGACY_COPYRIGHT = "Copyright (C) 2026 CantorAI"
BODY = [
    COPYRIGHT,
    'Licensed under the Apache License, Version 2.0 (the "License");',
    "you may not use this file except in compliance with the License.",
    "You may obtain a copy of the License at",
    "",
    "    http://www.apache.org/licenses/LICENSE-2.0",
    "",
    "Unless required by applicable law or agreed to in writing, software",
    'distributed under the License is distributed on an "AS IS" BASIS,',
    "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.",
    "See the License for the specific language governing permissions and",
    "limitations under the License.",
]


def block(prefix: str, suffix: str = "") -> str:
    if prefix == "/*":
        return "/*\n" + "\n".join(BODY) + "\n*/\n\n"
    if prefix == "<!--":
        return "<!--\n" + "\n".join(BODY) + "\n-->\n\n"
    lines = [prefix + (" " + line if line else "") for line in BODY]
    return "\n".join(lines) + ("\n" + suffix if suffix else "") + "\n\n"


def style(path: Path) -> tuple[str, str] | None:
    name = path.name
    extension = path.suffix.lower()
    if extension in {".c", ".cc", ".cpp", ".h", ".hpp", ".js"}:
        return "/*", ""
    if extension in {".py", ".cmake", ".yml", ".yaml"} or name in {"CMakeLists.txt", ".gitignore"}:
        return "#", ""
    if extension in {".md", ".html"}:
        return "<!--", "-->"
    if extension == ".tex":
        return "%", ""
    if extension in {".bat", ".cmd"}:
        return "REM", ""
    if extension == ".ps1":
        return "#", ""
    return None


def repository_files() -> list[Path]:
    tracked = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=ROOT,
    ).decode("utf-8").split("\0")
    return [ROOT / item for item in tracked if item]


def main() -> None:
    changed = 0
    for path in repository_files():
        if ROOT / "third_party" in path.parents:
            continue
        selected = style(path)
        if selected is None or not path.is_file() or path == Path(__file__):
            continue
        content = path.read_bytes().decode("utf-8-sig").replace("\ufeff", "")
        if LEGACY_COPYRIGHT in content[:2048]:
            updated = content.replace(LEGACY_COPYRIGHT, COPYRIGHT, 1)
            if updated != content:
                path.write_bytes(updated.encode("utf-8"))
                changed += 1
            continue
        if COPYRIGHT in content[:2048]:
            continue
        prefix, suffix = selected
        path.write_text(block(prefix, suffix) + content, encoding="utf-8", newline="\n")
        changed += 1
    print(f"Applied CantorAI license headers to {changed} files.")


if __name__ == "__main__":
    main()
