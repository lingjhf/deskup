"""Check whether this exact package version is already published on pub.dev."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
from urllib.error import HTTPError
from urllib.request import urlopen


def verify_archive(root, archive):
    root = Path(root)
    published = set()
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as bundle:
        for member in bundle.getmembers():
            name = PurePosixPath(member.name)
            if name.is_absolute() or '..' in name.parts:
                raise ValueError('Unsafe package archive path')
            if member.isdir():
                continue
            if not member.isfile():
                raise ValueError('Unsupported package archive entry: ' + member.name)
            path = root.joinpath(*name.parts)
            if not path.is_file() or path.read_bytes() != bundle.extractfile(member).read():
                raise ValueError('Published version differs from this commit: ' + member.name)
            published.add(name.as_posix())
    tracked = subprocess.check_output(
        ['git', 'ls-files', '-z', '--', 'lib', 'android', 'ios', 'linux', 'macos', 'windows'],
        cwd=root).decode().split('\0')
    for name in filter(None, tracked):
        # Pub omits hidden metadata such as platform .gitignore files.
        if any(part.startswith('.') for part in PurePosixPath(name).parts):
            continue
        if name not in published:
            raise ValueError('Published version is missing source file: ' + name)
    for name in ['pubspec.yaml', 'README.md', 'CHANGELOG.md', 'LICENSE']:
        if name not in published:
            raise ValueError('Published version is missing ' + name)


def already_published(root):
    root = Path(root)
    spec = (root / 'pubspec.yaml').read_text()
    name = re.search(r'^name:\s*([a-z][a-z0-9_]*)\s*$', spec, re.M).group(1)
    version = re.search(r'^version:\s*([^\s#]+)', spec, re.M).group(1).strip("'\"")
    url = f'https://pub.dev/api/packages/{name}/versions/{version}'
    try:
        with urlopen(url, timeout=30) as response:
            metadata = json.load(response)
    except HTTPError as error:
        if error.code == 404:
            return False
        raise
    archive_url = metadata['archive_url']
    if not archive_url.startswith('https://pub.dev/api/archives/'):
        raise ValueError('Unexpected pub.dev archive URL')
    with urlopen(archive_url, timeout=60) as response:
        archive = response.read()
    if hashlib.sha256(archive).hexdigest() != metadata['archive_sha256']:
        raise ValueError('Published archive checksum mismatch')
    verify_archive(root, archive)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--github-output', type=Path)
    args = parser.parse_args()
    exists = already_published(Path(__file__).resolve().parents[1])
    print('Published package matches this commit' if exists else 'Version is not published yet')
    if args.github_output:
        with args.github_output.open('a') as output:
            output.write('exists=' + str(exists).lower() + '\n')


if __name__ == '__main__':
    main()
