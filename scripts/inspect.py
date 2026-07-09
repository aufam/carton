#!/usr/bin/env python3

import json
import os
import re
import sys
import urllib.request


def fail(msg):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def github_repo(url) -> tuple[str, str] | None:
    """
    Returns (owner, repo) from:
      https://github.com/user/repo
      https://github.com/user/repo.git
      git@github.com:user/repo.git
    """
    patterns = [
        r"https://github\.com/([^/]+)/([^/]+?)(?:\.git)?/?$",
        r"git@github\.com:([^/]+)/([^/]+?)(?:\.git)?$",
    ]

    for p in patterns:
        m = re.match(p, url)
        if m:
            return m.group(1), m.group(2)

    return None


def github_api(owner, repo):
    req = urllib.request.Request(
        f"https://api.github.com/repos/{owner}/{repo}",
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "carton-inspect",
        },
    )

    token = os.getenv("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")

    with urllib.request.urlopen(req) as r:
        return json.load(r)


def repo_metadata(owner, repo):
    req = urllib.request.Request(
        f"https://api.github.com/repos/{owner}/{repo}/contents",
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "carton-inspect",
        },
    )

    token = os.getenv("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")

    with urllib.request.urlopen(req) as r:
        contents = json.load(r)

    readme = None
    if "README.md" in contents:
        readme = "README.md"

    license_file = None
    if "LICENSE" in contents:
        license_file = "LICENSE"

    for entry in contents:
        if entry.get("type") != "file":
            continue

        name = entry["name"]
        lower = name.lower()

        if readme is None and lower.startswith("readme"):
            readme = name

        if license_file is None and (
            lower.startswith("license")
            or lower.startswith("licence")
            or lower.startswith("copying")
            or lower == "unlicense"
        ):
            license_file = name

    return readme, license_file


_VERSION_RE = re.compile(r"(\d+\.\d+(?:\.\d+)?)")


def tags_metadata(owner, repo):
    req = urllib.request.Request(
        f"https://api.github.com/repos/{owner}/{repo}/tags",
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "carton-inspect",
        },
    )

    token = os.getenv("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")

    with urllib.request.urlopen(req) as r:
        tags = json.load(r)

    versions = []
    for tag in tags:
        m = _VERSION_RE.search(tag["name"])
        if m:
            versions.append(m.group(1))

    def version_key(v):
        return tuple(map(int, v.split(".")))

    return sorted(set(versions), key=version_key, reverse=True)


def emit(package):
    print("[package]")

    order = [
        "name",
        "authors",
        "description",
        "homepage",
        "repository",
        "license",
        "license-file",
        "readme",
        "keywords",
        "versions",
    ]

    for key in order:
        if key not in package:
            continue

        value = package[key]

        if isinstance(value, list):
            if len(value) <= 3:
                quoted = ", ".join(f'"{x}"' for x in value)
                print(f"{key} = [{quoted}]")
            else:
                quoted = ",\n    ".join(f'"{x}"' for x in value)
                print(f"{key} = [\n    {quoted},\n]")
        else:
            print(f'{key} = "{value}"')


def main():
    if len(sys.argv) != 2:
        print("usage: inspect.py <github-url>")
        sys.exit(1)

    url = sys.argv[1]

    parsed = github_repo(url)
    if parsed is None:
        fail("not a GitHub repository")
        return

    owner, repo = parsed

    info = github_api(owner, repo)

    package = {
        "name": repo,
        "authors": [owner],
        "repository": info["html_url"],
    }

    if info.get("description"):
        package["description"] = info["description"]

    if info.get("homepage"):
        package["homepage"] = info["homepage"]

    if info.get("license") and info["license"].get("spdx_id"):
        spdx = info["license"]["spdx_id"]
        if spdx != "NOASSERTION":
            package["license"] = spdx

    if info.get("topics"):
        package["keywords"] = info["topics"]

    readme, license_file = repo_metadata(owner, repo)
    if readme:
        package["readme"] = readme
    if license_file:
        package["license-file"] = license_file

    tags = tags_metadata(owner, repo)
    if len(tags) > 0:
        package["versions"] = tags

    emit(package)


if __name__ == "__main__":
    main()
