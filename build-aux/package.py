"""Build the private, reviewable CVA VEIL binary/source distribution pair.

Run from the shared workspace after build/test verification. This script only
packages an existing build; it never compiles, installs, starts OBS, reads OBS
configuration, uploads anything, or changes repository source files.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import shutil
import stat
import subprocess
import tempfile
from urllib.parse import unquote, urlsplit
import zipfile


PRODUCT = "CVA-VEIL-Auto-Sound-Setting"
VERSION = "1.3.0"
BINARY_NAME = f"{PRODUCT}-{VERSION}-windows-x64.zip"
SOURCE_NAME = f"{PRODUCT}-{VERSION}-source.zip"
SOURCE_PREFIX = f"{PRODUCT}-{VERSION}-source"
UPSTREAM = "https://github.com/AllegroMoltoV/obs-loudness-balance-monitor"
UPSTREAM_COMMIT = "de4297086b28efdf5ac0990dd795cdb20b20d2c2"
MAX_FILE_BYTES = 32 * 1024 * 1024
MAX_SOURCE_BYTES = 128 * 1024 * 1024
MAX_BINARY_BYTES = 32 * 1024 * 1024
SOURCE_ROOTS = frozenset({"src", "data", ".github", "cmake", "build-aux", "docs", "tools", "third-party"})
SOURCE_ROOT_FILES = frozenset({
    "CMakeLists.txt", "CMakePresets.json", "buildspec.json", "README.md", "LICENSE",
    "THIRD-PARTY-NOTICES.md", "MODIFICATIONS.md", ".gitignore", ".clang-format", ".gersemirc",
})
TOOLS_FILES = frozenset({"README.md", "prepare_portable_toolchain.py", "Enter-TRUEITEMBuild.ps1", "vs-catalog.json"})
FORBIDDEN_DIRS = frozenset({
    ".git", ".deps", ".codex-work", "__pycache__", ".pytest_cache", ".runtime", ".vs",
    "node_modules", "logs", "reports", "outputs", "work", "screenshots", "evidence",
    "obs-studio", "plugin_config", "profiles", "scenes", "secrets", "credentials",
})
FORBIDDEN_SUFFIXES = frozenset({
    ".dll", ".exe", ".pdb", ".obj", ".o", ".exp", ".lib", ".a", ".so", ".dylib",
    ".pyc", ".pyo", ".whl", ".msi", ".msix", ".cab", ".nupkg", ".zip", ".7z",
    ".tar", ".gz", ".xz", ".bz2", ".zst", ".dmp", ".log", ".wav", ".mp3", ".flac",
    ".ogg", ".m4a", ".mp4", ".mkv", ".mov", ".webm", ".sqlite", ".db", ".pem", ".key",
})
FORBIDDEN_NAMES = frozenset({
    "global.ini", "user.ini", "service.json", "assistant.json", "settings.json",
    "last-measurement.json", "last-adjustment.json", "obswebsocketconfig.json",
    "cookies", "login data", "local state", "manifest.json", "package-manifest.json",
    "source-manifest.json", ".env", ".netrc", "id_rsa", "id_ed25519",
})
IMAGE_SUFFIXES = frozenset({".png", ".jpg", ".jpeg", ".gif", ".webp", ".svg", ".ico", ".bmp"})
TEXT_SUFFIXES = frozenset({
    ".md", ".txt", ".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".cmake", ".json",
    ".yaml", ".yml", ".ini", ".py", ".ps1", ".cmd", ".cs", ".zsh", ".sh", ".effect", ".xml", ".qrc",
})
MARKDOWN_LINK = re.compile(r"!?\[[^\]\n]*\]\(([^\s)]+)(?:\s+\"[^\"]*\")?\)")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def git_names(repo: Path, *args: str) -> set[str]:
    result = subprocess.run(
        ["git", "-C", str(repo), "ls-files", "-z", *args],
        check=True, capture_output=True,
    )
    return {entry.decode("utf-8") for entry in result.stdout.split(b"\0") if entry}


def safe_relative(name: str) -> PurePosixPath:
    relative = PurePosixPath(name)
    if (relative.is_absolute() or str(relative) != name or not relative.parts or
            any(part in {"", ".", ".."} for part in relative.parts) or "\\" in name or ":" in name):
        raise ValueError(f"Unsafe repository/package path: {name!r}")
    return relative


def source_allowed(name: str, tracked: set[str]) -> tuple[bool, str]:
    relative = safe_relative(name)
    parts = relative.parts
    lowered = tuple(part.casefold() for part in parts)
    if len(parts) == 1:
        return name in SOURCE_ROOT_FILES, "root allowlist"
    if parts[0] not in SOURCE_ROOTS:
        return False, "outside source roots"
    if any(part in FORBIDDEN_DIRS for part in lowered):
        return False, "generated/private/work directory"
    if any(part.startswith("build") and part != "build-aux" for part in lowered[:-1]):
        return False, "generated build directory"
    if relative.name.casefold() in FORBIDDEN_NAMES or relative.name.casefold().startswith(".env."):
        return False, "configuration/credential/generated manifest name"
    if relative.suffix.casefold() in FORBIDDEN_SUFFIXES:
        return False, "binary/download/audio/log artifact"
    if parts[0] == "tools" and (len(parts) != 2 or parts[1] not in TOOLS_FILES):
        return False, "non-source tools payload"
    if name == "docs/INDEX.md":
        return False, "historical work-record index"
    if relative.suffix.casefold() in IMAGE_SUFFIXES:
        if not (name in tracked and name.startswith("docs/images/")):
            return False, "unapproved screenshot/image"
    return True, "source allowlist"


def load_stable_file(path: Path, repo: Path | None = None) -> bytes:
    if path.is_symlink():
        raise ValueError(f"Symlinks are not packaged: {path.name}")
    resolved = path.resolve(strict=True)
    if repo is not None:
        try:
            resolved.relative_to(repo)
        except ValueError as error:
            raise ValueError("A repository candidate resolves outside the source root") from error
    before = resolved.stat()
    if not stat.S_ISREG(before.st_mode) or before.st_size > MAX_FILE_BYTES:
        raise ValueError(f"Not an ordinary file within the {MAX_FILE_BYTES} byte limit: {path.name}")
    data = resolved.read_bytes()
    after = resolved.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns) or len(data) != before.st_size:
        raise RuntimeError(f"File changed during packaging; finish edits/build first: {path.name}")
    return data


def check_private_paths(name: str, data: bytes, repo: Path) -> None:
    relative = PurePosixPath(name)
    if relative.suffix.casefold() not in TEXT_SUFFIXES and relative.name not in SOURCE_ROOT_FILES:
        return
    text = data.decode("utf-8-sig", errors="strict")
    # Inspect only already-allowlisted public source text, not home files. Stop
    # rather than silently editing source bytes or publishing machine paths.
    private_roots = [str(repo), str(repo.parent.parent)]
    user_profile = os.environ.get("USERPROFILE")
    if user_profile:
        private_roots.append(user_profile)
    normalized = text.replace("\\\\", "\\").replace("\\", "/").casefold()
    if any(root.replace("\\", "/").casefold() in normalized for root in private_roots):
        raise ValueError(f"Local absolute path found in candidate source: {name}; remove it before packaging")


def validate_links(files: dict[str, bytes], *, only: set[str] | None = None) -> int:
    checked = 0
    for name, data in files.items():
        if not name.endswith(".md") or (only is not None and name not in only):
            continue
        text = data.decode("utf-8-sig")
        # Code examples can contain Markdown-shaped literals, so do not treat
        # fenced example code as document navigation.
        text = re.sub(r"(?ms)^```[^\n]*\n.*?^```\s*$", "", text)
        for match in MARKDOWN_LINK.finditer(text):
            link = match.group(1)
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            target = unquote(parsed.path)
            if target.startswith("/"):
                raise ValueError(f"Absolute local document link in {name}")
            resolved = posixpath.normpath(posixpath.join(posixpath.dirname(name), target))
            if resolved == ".." or resolved.startswith("../"):
                raise ValueError(f"Document link leaves distribution: {name} -> {link}")
            if resolved not in files and not any(item.startswith(resolved.rstrip("/") + "/") for item in files):
                raise ValueError(f"Document link points outside selected source/package: {name} -> {link}")
            checked += 1
    return checked


def file_entries(files: dict[str, bytes]) -> list[dict[str, object]]:
    return [{"path": name, "bytes": len(data), "sha256": digest(data)} for name, data in sorted(files.items())]


def write_zip(path: Path, files: dict[str, bytes]) -> None:
    if len({name.casefold() for name in files}) != len(files):
        raise ValueError("Case-insensitive duplicate package paths")
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            safe_relative(name)
            info = zipfile.ZipInfo(name, (2026, 11, 9, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o100644 << 16)
            archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or set(names) != set(files) or archive.testzip() is not None:
            raise RuntimeError("ZIP entry/CRC verification failed")
        for name in names:
            if digest(archive.read(name)) != digest(files[name]):
                raise RuntimeError(f"ZIP content readback failed: {name}")


def replace_pair(staged_source: Path, staged_binary: Path, destination: Path) -> None:
    # Only the two explicitly named artifacts are replaced. Keep rollback copies
    # until both moves succeed, so a second replacement error restores the pair.
    prior: dict[Path, Path | None] = {}
    committed: list[Path] = []
    for name in (SOURCE_NAME, BINARY_NAME):
        target = destination / name
        if target.is_symlink() or (target.exists() and not target.is_file()):
            raise ValueError(f"Refusing non-file package destination: {name}")
        backup = staged_source.parent / ("previous-" + name) if target.exists() else None
        if backup is not None:
            shutil.copy2(target, backup)
        prior[target] = backup
    try:
        for staged, name in ((staged_source, SOURCE_NAME), (staged_binary, BINARY_NAME)):
            target = destination / name
            os.replace(staged, target)
            committed.append(target)
    except BaseException:
        for target in reversed(committed):
            backup = prior[target]
            if backup is not None:
                os.replace(backup, target)
            elif target.is_file():
                target.unlink()
        raise


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    default_repo = Path(__file__).resolve().parents[1]
    parser.add_argument("--repo", type=Path, default=default_repo)
    parser.add_argument("--output-dir", type=Path, default=default_repo / "dist")
    parser.add_argument("--check", action="store_true", help="Validate candidates and links without creating archives")
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    output = args.output_dir.resolve()
    if not (repo / ".git").exists():
        raise ValueError("Source must be the intended Git checkout")
    if output == repo or (repo in output.parents and output != repo / "dist"):
        raise ValueError("Archives must use the ignored dist folder or an external output folder")
    tracked = git_names(repo, "--cached")
    candidates = tracked | git_names(repo, "--others", "--exclude-standard")
    sources: dict[str, bytes] = {}
    excluded: dict[str, int] = {}
    for name in sorted(candidates):
        allowed, reason = source_allowed(name, tracked)
        if not allowed:
            excluded[reason] = excluded.get(reason, 0) + 1
            continue
        data = load_stable_file(repo.joinpath(*PurePosixPath(name).parts), repo)
        check_private_paths(name, data, repo)
        sources[name] = data
    required_source = {
        "CMakeLists.txt", "CMakePresets.json", "buildspec.json", "README.md", "LICENSE",
        "THIRD-PARTY-NOTICES.md", "MODIFICATIONS.md", "docs/TRUEITEM-사용안내.md",
        "build-aux/TRUEITEM-Install.ps1", "build-aux/CVA-VEIL-InstallCore.ps1",
        "build-aux/CVA-VEIL-Setup.ps1", "build-aux/CVA-VEIL-Setup.cmd", "data/i18n.json",
        "tools/README.md", "tools/vs-catalog.json",
        "tools/prepare_portable_toolchain.py", "tools/Enter-TRUEITEMBuild.ps1",
        "third-party/libebur128-LICENSE.txt", "third-party/libebur128/COPYING",
        "third-party/libebur128/CMakeLists.txt", "third-party/libebur128/ebur128/ebur128.c",
        "third-party/libebur128/ebur128/ebur128.h",
    }
    if required_source - sources.keys():
        raise ValueError("Required corresponding source missing: " + ", ".join(sorted(required_source - sources.keys())))
    source_bytes = sum(map(len, sources.values()))
    if source_bytes > MAX_SOURCE_BYTES:
        raise ValueError(f"Selected source exceeds {MAX_SOURCE_BYTES} bytes; review the source allowlist")
    spec = json.loads(sources["buildspec.json"])
    if spec.get("version") != VERSION or spec.get("name") != "loudness-balance-monitor":
        raise ValueError("Build metadata does not match the requested package version/module")
    if b"GNU GENERAL PUBLIC LICENSE" not in sources["LICENSE"]:
        raise ValueError("GPL license text is missing")
    notice = sources["THIRD-PARTY-NOTICES.md"].decode("utf-8-sig")
    if UPSTREAM_COMMIT not in notice or "AllegroMoltoV" not in notice:
        raise ValueError("Upstream attribution/source basis missing")
    source_link_count = validate_links(sources)
    dll_path = repo / "build_portable_multi/RelWithDebInfo/loudness-balance-monitor.dll"
    dll = load_stable_file(dll_path, repo)
    build_source_times = [repo.joinpath(*PurePosixPath(name).parts).stat().st_mtime_ns for name in sources
                          if name.startswith("src/") and PurePosixPath(name).suffix in {".c", ".cpp", ".h", ".hpp"}]
    if build_source_times and dll_path.stat().st_mtime_ns < max(build_source_times):
        raise ValueError("DLL predates changed program source; rebuild/retest before packaging")
    if dll[:2] != b"MZ":
        raise ValueError("Plugin build is not a Windows PE file")
    runtime: dict[str, bytes] = {"loudness-balance-monitor/loudness-balance-monitor.dll": dll}
    for locale in ("en-US.ini", "ja-JP.ini", "ko-KR.ini"):
        name = "data/locale/" + locale
        if name not in sources:
            raise ValueError("Required locale missing: " + locale)
        runtime["loudness-balance-monitor/" + name] = sources[name]
    runtime["CVA-VEIL-Install.ps1"] = sources["build-aux/TRUEITEM-Install.ps1"]
    runtime["loudness-balance-monitor/data/i18n.json"] = sources["data/i18n.json"]
    for name in ("CVA-VEIL-InstallCore.ps1", "CVA-VEIL-Setup.ps1", "CVA-VEIL-Setup.cmd"):
        runtime[name] = sources["build-aux/" + name]
    setup_host = load_stable_file(repo / "work/installer-host-final/CVA-VEIL-Setup.exe", repo)
    if setup_host[:2] != b"MZ":
        raise ValueError("Installer host is not a Windows executable")
    runtime["CVA-VEIL-Setup.exe"] = setup_host
    for name in ("LICENSE", "THIRD-PARTY-NOTICES.md", "MODIFICATIONS.md", "third-party/libebur128-LICENSE.txt", "docs/portable-obs-sdk-patch.md", "tools/README.md"):
        runtime[name] = sources[name]
    for name in ("build-aux/sdk-cache/use-prebuilt-sdk.cmake", "build-aux/sdk-cache/cmake/buildspec.cmake", "build-aux/INSTALLER.md"):
        runtime[name] = sources[name]
    runtime["MODIFICATIONS.md"] = sources["MODIFICATIONS.md"].decode("utf-8-sig").replace(
        "](docs/TRUEITEM-사용안내.md)", "](README.md)"
    ).encode("utf-8")
    usage = sources["docs/TRUEITEM-사용안내.md"].decode("utf-8-sig")
    usage = usage.replace("](../THIRD-PARTY-NOTICES.md)", "](THIRD-PARTY-NOTICES.md)")
    usage = usage.replace("](../MODIFICATIONS.md)", "](MODIFICATIONS.md)")
    usage += ("\n\n## 이 패키지에 대응하는 소스\n\n"
              f"같은 배포 위치의 **{SOURCE_NAME}**가 이 버전의 소스·빌드자료 묶음입니다.\n"
              "정확한 파일 해시는 `CORRESPONDING-SOURCE.txt` 및 `package-manifest.json`에 기록됩니다.\n"
              "라이선스·원작 고지는 함께 제공된 문서를 유지하세요.\n")
    runtime["README.md"] = usage.encode("utf-8")
    if sum(map(len, runtime.values())) > MAX_BINARY_BYTES:
        raise ValueError("Binary package inputs exceed the review limit")
    binary_link_count = validate_links(runtime)
    summary = {
        "product": PRODUCT, "version": VERSION, "mode": "check" if args.check else "package",
        "source_files": len(sources), "source_uncompressed_bytes": source_bytes,
        "max_source_file_bytes": MAX_FILE_BYTES, "max_source_total_bytes": MAX_SOURCE_BYTES,
        "source_document_links_checked": source_link_count, "binary_readme_links_checked": binary_link_count,
        "excluded_candidates_by_reason": excluded, "dll_sha256": digest(dll),
        "limits": "Allowlist, stable read, hashes, ZIP readback and document links only; not an independent build, license, privacy or runtime audit.",
        "auth_and_obs_settings": "Not searched, copied, or packaged", "uploaded": False,
    }
    if args.check:
        print(json.dumps(summary, ensure_ascii=False, indent=2))
        return
    output.mkdir(parents=True, exist_ok=True)
    created = dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).isoformat()
    source_manifest = {
        "schema": "cva-veil-corresponding-source.v1", "product": PRODUCT, "version": VERSION,
        "created_at_kst": created, "upstream": UPSTREAM, "upstream_tag": "v1.0.2",
        "upstream_commit": UPSTREAM_COMMIT, "binary_name": BINARY_NAME,
        "files": file_entries(sources), "build_instructions": "tools/README.md",
        "source_selection": "Git tracked plus non-ignored untracked files, constrained by explicit source roots and exclusions",
        "limits": summary["limits"],
    }
    source_payload = {SOURCE_PREFIX + "/" + name: data for name, data in sources.items()}
    source_payload[SOURCE_PREFIX + "/source-manifest.json"] = json_bytes(source_manifest)
    with tempfile.TemporaryDirectory(prefix=".cva-veil-package-", dir=output) as staging_name:
        staging = Path(staging_name)
        source_zip = staging / SOURCE_NAME
        binary_zip = staging / BINARY_NAME
        write_zip(source_zip, source_payload)
        source_zip_hash = digest(source_zip.read_bytes())
        runtime["CORRESPONDING-SOURCE.txt"] = (
            f"CVA-VEIL-Auto Sound Setting {VERSION}\n"
            f"Corresponding source archive: {SOURCE_NAME}\n"
            f"SHA256: {source_zip_hash}\n"
            f"Archive bytes: {source_zip.stat().st_size}\n"
            f"Source root in archive: {SOURCE_PREFIX}/\n"
            "Build instructions: tools/README.md in that source archive.\n"
            "Provide this exact source archive alongside the Windows archive.\n"
            "GPL-2.0 license and upstream attribution are retained.\n"
        ).encode("utf-8")
        package_manifest = {
            "schema": "cva-veil-plugin-package.v1", "product": PRODUCT, "version": VERSION,
            "created_at_kst": created, "upstream": UPSTREAM, "upstream_commit": UPSTREAM_COMMIT,
            "files": file_entries(runtime),
            "corresponding_source": {"filename": SOURCE_NAME, "sha256": source_zip_hash,
                                     "bytes": source_zip.stat().st_size, "root": SOURCE_PREFIX},
            "source_manifest_sha256": digest(json_bytes(source_manifest)),
            "binary_readme_derived_from": "docs/TRUEITEM-사용안내.md",
            "binary_document_adaptations": "Relative links adapted to the flat binary-package documents; source documents retained unchanged in the source archive",
            "build_input_timestamp_check": "PASS: plugin DLL is not older than selected src headers/implementation",
            "excluded_runtime": ["OBS", "Qt", "Python", "compiler", "Windows SDK", "PDB", "user OBS configuration"],
            "limits": summary["limits"],
        }
        runtime["package-manifest.json"] = json_bytes(package_manifest)
        # README and required source declaration must refer to this exact pair.
        if SOURCE_NAME not in runtime["README.md"].decode("utf-8") or SOURCE_NAME not in runtime["CORRESPONDING-SOURCE.txt"].decode("utf-8"):
            raise RuntimeError("Corresponding source filename is not linked by the binary documentation")
        validate_links(runtime)
        write_zip(binary_zip, runtime)
        result_archives = [{"filename": path.name, "bytes": path.stat().st_size,
                            "sha256": digest(path.read_bytes())} for path in (binary_zip, source_zip)]
        replace_pair(source_zip, binary_zip, output)
    summary["archives"] = result_archives
    summary["source_manifest_files"] = len(source_manifest["files"])
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
