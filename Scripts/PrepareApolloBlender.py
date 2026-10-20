"""Download and verify the official portable Blender used by Apollo FBX export."""
from pathlib import Path
import hashlib
import json
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tmp/apollo-tools'
EVIDENCE = ROOT / 'Feature/doc/evidence/arin-apollo-20261005'
VERSION = '4.5.14'
NAME = f'blender-{VERSION}-windows-x64.zip'
BASE = 'https://ftp.nluug.nl/pub/graphics/blender/release/Blender4.5/'
DOWNLOAD_BASE = 'https://ftp.nluug.nl/pub/graphics/blender/release/Blender4.5/'


def request(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers={
        'User-Agent': 'Mozilla/5.0'}), timeout=60)


def main():
    TOOLS.mkdir(parents=True, exist_ok=True)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    checksums = request(BASE + f'blender-{VERSION}.sha256').read().decode()
    (EVIDENCE / 'blender-official.sha256').write_text(checksums, encoding='utf-8')
    matching = [line.split()[0] for line in checksums.splitlines() if NAME in line]
    if len(matching) != 1:
        raise RuntimeError('Official ZIP checksum could not be verified')
    archive = TOOLS / NAME
    if not archive.exists():
        part = archive.with_suffix('.zip.part')
        count = 0
        with request(DOWNLOAD_BASE + NAME) as response, part.open('wb') as stream:
            while chunk := response.read(4 * 1024 * 1024):
                stream.write(chunk)
                count += len(chunk)
                if count % (32 * 1024 * 1024) == 0:
                    print(f'Blender download: {count // (1024 * 1024)} MiB', flush=True)
        part.replace(archive)
    digest = hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest()
    if digest != matching[0]:
        raise RuntimeError('Portable archive SHA256 does not match official checksum')
    executable = TOOLS / NAME.removesuffix('.zip') / 'blender.exe'
    if not executable.is_file():
        with zipfile.ZipFile(archive) as package:
            for member in package.infolist():
                destination = (TOOLS / member.filename).resolve()
                if not destination.is_relative_to(TOOLS.resolve()):
                    raise RuntimeError('Unsafe archive member path')
            package.extractall(TOOLS)
    if not executable.is_file():
        raise RuntimeError('Portable Blender executable is missing')
    report = {'version': VERSION, 'url': DOWNLOAD_BASE + NAME, 'sha256': digest,
              'official_checksum_matches': True, 'executable': str(executable)}
    (EVIDENCE / 'blender-install.json').write_text(
        json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
