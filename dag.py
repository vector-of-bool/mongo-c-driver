import os
import platform
import re
import sys
from pathlib import Path
from typing import IO, AsyncIterable, Optional

import aiohttp
from dagon import ar, fs, option, proc, task, ui

HERE = Path(__file__).parent.resolve()

PROG_RE = re.compile(r'\b(\d+)/(\d+)\b')

use_system_cmake = option.add('use-system-cmake',
                              type=bool,
                              doc='If true, use the system CMake instead of downloading one',
                              default=False)

cmake_version = option.add('cmake-version',
                           type=str,
                           doc='The version number of the CMake to download',
                           default='3.23.1')

caches_dir = option.add('cache-dir',
                        type=Path,
                        doc='Directory in which to store caches',
                        default={
                            'nt': lambda: Path(os.environ['LocalAppData']) / 'mongo/c',
                            'posix': lambda: Path(os.environ.get('XDG_CACHE_HOME', '~/.cache/')) / 'mongo/c',
                            'darwin': lambda: Path('~/Library/Caches') / 'mongo/c',
                        }[os.name]().expanduser())

build_dir = option.add('build-dir',
                       type=Path,
                       doc='Directory in which to store the CMake configure/build data',
                       default=HERE / '_build')

ninja_version = option.add('ninja-version', type=str, doc='The version of Ninja to downlaod', default='1.10.2')


@task.define()
async def clean():
    "Delete prior build results"
    await fs.remove(build_dir.get(), recurse=True, absent_ok=True)


async def _write_to(src: AsyncIterable[bytes], to: IO[bytes], length: Optional[int]):
    nread = 0
    async for part in src:
        nread += len(part)
        to.write(part)
        if length is not None:
            ui.progress(nread / length)


async def _download(url: str, dest: Path, ignore_existing=False) -> None:
    if dest.is_file() and not ignore_existing:
        return

    tmp = dest.with_name(dest.name + '.tmp')
    async with aiohttp.ClientSession() as sess:
        ui.status(f'Downloading {url} ...')
        resp = await sess.get(url)
        resp.raise_for_status()
        tmp.parent.mkdir(exist_ok=True, parents=True)
        with tmp.open('wb') as out:
            await _write_to(resp.content.iter_any(), out, resp.content_length)
    tmp.rename(dest)


def _on_expand(ev: ar.ExtractInfo):
    ui.status(f'Extracted: {ev.ar_path}')
    ui.progress(ev.n / ev.total_count)


@task.define()
async def get_ninja() -> Path:
    "Obtain a Ninja executable for the build"
    prefix = caches_dir.get() / f'ninja-{ninja_version.get()}'
    build_dir = prefix / '_build'
    ninja_bin = build_dir / ('ninja' + ('.exe' if os.name == 'nt' else ''))
    if ninja_bin.is_file():
        return ninja_bin

    url = f'https://github.com/ninja-build/ninja/archive/refs/tags/v{ninja_version.get()}.tar.gz'
    src_tgz = prefix / 'ninja-src.tgz'
    await _download(url, src_tgz)

    await fs.remove(build_dir, absent_ok=True, recurse=True)
    ui.status('Expanding')
    ev = ar.ExtractFileEvent()
    ev.connect(_on_expand)
    await ar.expand(src_tgz, destination=build_dir, on_extract=ev, strip_components=1)
    os.chmod(build_dir / 'src/inline.sh', 0o755)
    ui.status('Bootstrapping Ninja binary...')
    ui.progress(None)
    await proc.run([sys.executable, '-u', build_dir / 'configure.py', '--bootstrap'],
                   cwd=build_dir,
                   on_output=_progress_status)
    return build_dir / ('ninja' + ('.exe' if os.name == 'nt' else ''))


@task.define()
async def get_cmake() -> Path:
    "Obtain a CMake executable for the build"
    if use_system_cmake.get():
        return Path('cmake')

    plat = {
        'nt': 'windows',
        'darwin': 'macos',
        'posix': 'linux',
    }[os.name]
    version = cmake_version.get()
    arch = platform.machine()
    arch = {
        'windows': {
            'AMD64': 'x86_64',
            'i386': 'i386',
        },
        'linux': {
            arch: arch,
        },
        'macos': {
            arch: 'universal',
        },
    }[plat][arch]
    ext = {'windows': 'zip'}.get(os.name, 'tar.gz')
    filename = f'cmake-{version}-{plat}-{arch}.{ext}'
    url = f'https://github.com/Kitware/CMake/releases/download/v{version}/{filename}'
    dest = caches_dir.get() / filename
    await _download(url, dest)

    prefix = caches_dir.get() / f'cmake-{version}-{arch}'
    cmake_bin = prefix / ('bin/cmake' + ('.exe' if os.name == 'nt' else ''))
    if cmake_bin.is_file():
        return cmake_bin

    tmp = prefix.with_name(prefix.name + '.tmp')
    ui.status(f'Removing prior expanded archive')
    await fs.remove([prefix, tmp], recurse=True, absent_ok=True)
    ui.status(f'Expanding archive {filename}')
    ev = ar.ExtractFileEvent()
    ev.connect(_on_expand)
    await ar.expand(dest, destination=tmp, strip_components=1, on_extract=ev)
    tmp.rename(prefix)
    assert cmake_bin.is_file(), cmake_bin
    ui.print(f'Extracted CMake: [{cmake_bin}]')
    os.chmod(cmake_bin, 0o755)
    return cmake_bin


def _progress_status(message: proc.ProcessOutputItem) -> None:
    line = message.out.decode()
    ui.status(line)
    mat = PROG_RE.search(line)
    if not mat:
        return
    num, den = mat.groups()
    prog = int(num) / int(den)
    ui.progress(prog)


@task.define(order_only_depends=[clean], depends=[get_cmake, get_ninja])
async def configure() -> None:
    cmake_bin = await task.result_of(get_cmake)
    ninja_bin = await task.result_of(get_ninja)

    await proc.run(
        [
            cmake_bin,
            f'-H{HERE}',
            f'-B{build_dir.get()}',
            f'-GNinja Multi-Config',
            f'-DCMAKE_MAKE_PROGRAM={ninja_bin}',
        ],
        on_output='print',
    )


@task.define(depends=[configure, get_cmake])
async def build() -> None:
    cmake_bin = await task.result_of(get_cmake)
    await proc.run([cmake_bin, '--build', build_dir.get()], on_output=_progress_status, print_output_on_finish='always')
