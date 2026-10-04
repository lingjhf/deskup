"""Check package version consistency, and optionally validate a release tag."""
import argparse
from pathlib import Path
import re
import subprocess

NUMBER = r'(?:0|[1-9][0-9]*)'
PRERELEASE = r'(?:0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)'
SEMVER = re.compile(
    rf'{NUMBER}\.{NUMBER}\.{NUMBER}'
    rf'(?:-{PRERELEASE}(?:\.{PRERELEASE})*)?'
    r'(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?'
)


def validate(root, tag=None):
    root = Path(root)
    match = re.search(r'^version:\s*([^\s#]+)\s*(?:#.*)?$',
                      (root / 'pubspec.yaml').read_text(), re.M)
    version = match.group(1).strip("'\"") if match else ''
    if not SEMVER.fullmatch(version):
        raise ValueError('pubspec.yaml must declare a valid semantic version')
    pod = re.search(r's\.version\s*=\s*[\'\"]([^\'\"]+)[\'\"]',
                    (root / 'macos/deskup.podspec').read_text())
    if pod is None or pod.group(1) != version:
        raise ValueError('podspec version must match pubspec.yaml')
    if not re.search(r'^##\s+' + re.escape(version) + r'\s*$',
                     (root / 'CHANGELOG.md').read_text(), re.M):
        raise ValueError('CHANGELOG.md must contain a heading for this version')
    if tag is not None and tag != 'v' + version:
        raise ValueError('Release tag must be v' + version)
    return version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag')
    parser.add_argument('--check-main', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        version = validate(root, args.tag)
        if args.check_main:
            subprocess.run(['git', 'merge-base', '--is-ancestor', 'HEAD', 'origin/main'],
                           cwd=root, check=True)
    except (ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
    print('Version validated: ' + version)


if __name__ == '__main__':
    main()
