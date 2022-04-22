import subprocess
import json
import os
import platform
import re
import itertools
import sys
from pathlib import Path
import tempfile
from typing import IO, Any, AsyncIterable, Mapping, Optional

import aiohttp
from dagon import ar, fs, option, proc, task, ui, persist

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
                            'win32': lambda: Path(os.environ['LocalAppData']) / 'mongo/c',
                            'linux': lambda: Path(os.environ.get('XDG_CACHE_HOME', '~/.cache/')) / 'mongo/c',
                            'darwin': lambda: Path('~/Library/Caches') / 'mongo/c',
                        }[sys.platform]().expanduser())

build_dir = option.add('build-dir',
                       type=Path,
                       doc='Directory in which to store the CMake configure/build data',
                       default=HERE / '_build')

cmake_preseed = option.add('cmake-preseed',
                           type=Path,
                           doc='A CMake script to run before the CMake project is configured',
                           default=None)

ninja_version = option.add('ninja-version', type=str, doc='The version of Ninja to downlaod', default='1.10.2')

vs_version = option.add('vs-version',
                        type=str,
                        doc='Set the major Visual Studio version to load into the environment',
                        default=None)
vs_arch = option.add('vs-arch',
                     type=str,
                     doc='Set the Visual Studio build architecture to load into the environment',
                     default='amd64')

winsdk_version = option.add('winsdk-versin', type=str, doc='Set a specific Windows SDK version to load', default=None)

libmongocrypt_version = option.add('libmongocrypt-version',
                                   type=str,
                                   doc='The tag of libmongocrypt to download and build',
                                   default='1.4.0')


@task.define()
async def clean():
    "Delete prior build results"
    await fs.remove(build_dir.get(), recurse=True, absent_ok=True)


def _which(program: str) -> Optional[Path]:
    paths = os.environ['PATH'].split(os.pathsep)
    exts = os.environ.get('PATHEXT', '').split(os.pathsep)
    for pathdir, pathext in itertools.product(paths, exts):
        cand = Path(pathdir) / (program + pathext)
        if cand.is_file():
            return cand


@task.define()
async def calc_env() -> Mapping[str, str]:
    vs = vs_version.get()
    if vs is None:
        return dict(os.environ)

    vswhere = _which('vswhere')

    if vswhere is None:
        # Try to find the vshwere placed by the VS installer:
        progs = ('C:/Program Files/', 'C:/Program Files (x86)/')
        for pf in progs:
            cand = Path(pf) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            if cand.is_file():
                vswhere = cand

    if vswhere is None:
        raise RuntimeError(f'Cannot load Visual Studio environment without a "vswhere" program')

    ui.print(f'Using vswhere: [{vswhere}]')
    result = await proc.run([vswhere, '-nologo', '-utf8', ('-format', 'json'), '-all', ('-products', '*'), '-legacy'])
    vswhere_data = result.stdout_json()

    picked: Optional[Any] = None
    for cand in vswhere_data:
        install_version: str = cand['installationVersion']
        major = install_version.split('.')[0]
        if major == vs:
            picked = cand

    if picked is None:
        versions = [(f'{v["installationVersion"]} [{v.get("displayName", v["installationPath"])}]')
                    for v in vswhere_data]
        raise RuntimeError(f'No VS "{vs}" major version found. Available: {", ".join(versions)}')

    inst_path = Path(picked['installationPath'])
    ui.print(f'Using Visual Studio: [{inst_path}]')

    bats = ('vsdevcmd.bat', 'vcvarsall.bat')
    found = itertools.chain.from_iterable(inst_path.rglob(fname) for fname in bats)

    bat = next(iter(found))

    key = dict(os.environ)
    key['%vs_version'] = vs
    key['%vs_arch'] = vs_arch.get()
    key['%vars_file'] = str(bat)
    key = json.dumps(key, sort_keys=True)
    prev = persist.globl.get(key, None)
    if prev is not None:
        return prev

    ui.status(f'Loading VS environment [{inst_path}]')
    vars: Mapping[str, str] = {}
    if bat.name.lower() == 'vsdevcmd.bat':
        vars = await _vars_from_vsdevcmd(bat)
    if bat.name.lower() == 'vcvarsall.bat':
        vars = await _vars_from_vcvarsall(bat)

    assert vars
    persist.globl.set(key, vars)
    return vars


async def _vars_from_vsdevcmd(bat: Path) -> Mapping[str, str]:
    args: proc.CommandLine = [bat]
    args.append('-no_logo')
    args.append(f'-arch={vs_arch.get()}')
    args.append(f'-host_arch={vs_arch.get()}')
    sdk_ver = winsdk_version.get()
    if sdk_ver is not None:
        args.append(f'-winsdk={sdk_ver}')
    return await _vars_from_bat_cmd(args)


async def _vars_from_vcvarsall(bat: Path) -> Mapping[str, str]:
    args: proc.CommandLine = [bat]
    args.append(vs_arch.get())
    sdk_ver = winsdk_version.get()
    if sdk_ver is not None:
        args.append(sdk_ver)
    return await _vars_from_bat_cmd(args)


async def _vars_from_bat_cmd(args: proc.CommandLine) -> Mapping[str, str]:
    tmpdir = Path(tempfile.mkdtemp())
    try:
        tmpfile = tmpdir / 'load-vars.bat'
        args = proc.plain_commandline(args)
        tmpfile.write_text(fr'''
            @echo off
            call %*
            set
        ''')
        proc_res = await proc.run(['cmd', '/c', tmpfile, args])
    finally:
        await fs.remove(tmpdir, recurse=True)
    ret: Mapping[str, str] = {}
    for item in proc_res.output:
        if item.kind != 'output':
            continue
        key, val = item.out.rstrip().split(b'=', maxsplit=1)
        ret[key.decode()] = val.decode()
    return ret


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


def _on_extract_fn(ev: ar.ExtractInfo):
    ui.status(f'Extracted: {ev.ar_path}')
    ui.progress(ev.n / ev.total_count)


_on_extract = ar.ExtractFileEvent()
_on_extract.connect(_on_extract_fn)


@task.define(depends=[calc_env])
async def get_ninja() -> Path:
    "Obtain a Ninja executable for the build"
    prefix = caches_dir.get() / f'ninja-{ninja_version.get()}'
    build_dir = prefix / '_build'
    ninja_bin = build_dir / ('ninja' + ('.exe' if sys.platform == 'win32' else ''))
    if ninja_bin.is_file():
        return ninja_bin

    url = f'https://github.com/ninja-build/ninja/archive/refs/tags/v{ninja_version.get()}.tar.gz'
    src_tgz = prefix / 'ninja-src.tgz'
    await _download(url, src_tgz)

    await fs.remove(build_dir, absent_ok=True, recurse=True)
    ui.status('Expanding')

    await ar.expand(src_tgz, destination=build_dir, on_extract=_on_extract, strip_components=1)
    os.chmod(build_dir / 'src/inline.sh', 0o755)
    ui.status('Bootstrapping Ninja binary...')
    ui.progress(None)
    await proc.run([sys.executable, '-u', build_dir / 'configure.py', '--bootstrap'],
                   cwd=build_dir,
                   on_output=_ninja_progress,
                   env=await task.result_of(calc_env))
    return build_dir / ('ninja' + ('.exe' if sys.platform == 'win32' else ''))


@task.define()
async def get_cmake() -> Path:
    "Obtain a CMake executable for the build"
    if use_system_cmake.get():
        found = _which('cmake')
        if not found:
            raise RuntimeError('No "cmake" executable was found on the PATH')
        return found

    plat = {
        'win32': 'windows',
        'darwin': 'macos',
        'linux': 'linux',
    }[sys.platform]
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
    ext = {'win32': 'zip'}.get(sys.platform, 'tar.gz')
    filename = f'cmake-{version}-{plat}-{arch}.{ext}'
    url = f'https://github.com/Kitware/CMake/releases/download/v{version}/{filename}'
    dest = caches_dir.get() / filename
    await _download(url, dest)

    prefix = caches_dir.get() / f'cmake-{version}-{arch}'
    cmake_bin = prefix / ('bin/cmake' + ('.exe' if sys.platform == 'win32' else ''))
    if cmake_bin.is_file():
        return cmake_bin

    tmp = prefix.with_name(prefix.name + '.tmp')
    ui.status(f'Removing prior expanded archive')
    await fs.remove([prefix, tmp], recurse=True, absent_ok=True)
    ui.status(f'Expanding archive {filename}')
    await ar.expand(dest, destination=tmp, strip_components=1, on_extract=_on_extract)
    tmp.rename(prefix)
    assert cmake_bin.is_file(), cmake_bin
    ui.print(f'Extracted CMake: [{cmake_bin}]')
    os.chmod(cmake_bin, 0o755)
    return cmake_bin


def _ninja_progress(message: proc.ProcessOutputItem) -> None:
    line = message.out.decode()
    ui.status(line)
    mat = PROG_RE.search(line)
    if not mat:
        return
    num, den = mat.groups()
    prog = int(num) / int(den)
    ui.progress(prog)


@task.define(order_only_depends=[clean])
async def dl_libmongocrypt() -> Path:
    tag = libmongocrypt_version.get()
    tar_dest = build_dir.get() / f'libmongocrypt-{tag}.tar.gz'
    await _download(f'https://github.com/mongodb/libmongocrypt/archive/refs/tags/{tag}.tar.gz', tar_dest)
    root = build_dir.get() / f'libmongocrypt-{tag}'
    await fs.remove(root, recurse=True, absent_ok=True)
    await ar.expand(tar_dest, destination=root, on_extract=_on_extract, strip_components=1)
    return root


async def _cmake_configure(src_dir: Path, build_dir: Path, *, cmake_flags: proc.CommandLine) -> None:
    cmake = await task.result_of(get_cmake)
    ninja = await task.result_of(get_ninja)
    env = await task.result_of(calc_env)
    cmd = [
        cmake,
        f'-H{src_dir}',
        f'-B{build_dir}',
        '-GNinja Multi-Config',
        f'-DCMAKE_MAKE_PROGRAM={ninja}',
        cmake_flags,
    ]
    try:
        await proc.run(cmd, on_output='print', env=env)
    except subprocess.CalledProcessError as e:
        if b'No CMAKE_C_COMPILER could be found' in e.stderr and os.name == 'nt':
            ui.print('Note: No C compiler could be found. Did you mean to set a "vs-version" option?',
                     type=ui.MessageType.Warning)
        raise


async def _cmake_build(build_dir: Path,
                       *,
                       print_on_finish: proc._PrintOnFinishArg = 'on-fail',
                       install: bool = False,
                       config: str = 'Debug') -> None:
    cmake = await task.result_of(get_cmake)
    env = await task.result_of(calc_env)
    await proc.run([cmake, '--build', build_dir, f'--config={config}'],
                   env=env,
                   on_output=_ninja_progress,
                   print_output_on_finish=print_on_finish)
    if install:
        await proc.run([cmake, f'-DCMAKE_INSTALL_CONFIG_NAME={config}', '-P', build_dir / 'cmake_install.cmake'])


@task.define(order_only_depends=[clean], depends=[get_cmake, get_ninja, calc_env])
async def build_install_libbson_tmp() -> Path:
    src_dir = HERE
    tmp_dir = src_dir / '_build/_libbson-only'
    prefix = src_dir / '_build/_libbson-install'
    await _cmake_configure(src_dir, tmp_dir, cmake_flags=('-DENABLE_MONGOC=OFF', f'-DCMAKE_INSTALL_PREFIX={prefix}'))
    await _cmake_build(tmp_dir, install=True)
    return prefix


@task.define(order_only_depends=[clean],
             depends=[get_cmake, get_ninja, calc_env, dl_libmongocrypt, build_install_libbson_tmp])
async def build_libmongocrypt() -> Path:
    src_dir = await task.result_of(dl_libmongocrypt)
    bin_dir = src_dir / '_build/_libmongocrypt'
    prefix = src_dir / '_build/_libmongocrypt-intsall'
    env = await task.result_of(calc_env)
    cmake = await task.result_of(get_cmake)
    libbson_root = await task.result_of(build_install_libbson_tmp)
    await _cmake_configure(src_dir,
                           bin_dir,
                           cmake_flags=(f'-DCMAKE_INSTALL_PREFIX={prefix}', f'-DCMAKE_PREFIX_PATH={libbson_root}'))
    await _cmake_build(bin_dir, install=True)
    return prefix


@task.define(order_only_depends=[clean], depends=[get_cmake, get_ninja, calc_env, build_libmongocrypt])
async def configure() -> None:
    cmake_bin = await task.result_of(get_cmake)
    ninja_bin = await task.result_of(get_ninja)
    lmc_prefix = await task.result_of(build_libmongocrypt)
    await _cmake_configure(HERE, build_dir.get(), cmake_flags=(f'-DCMAKE_INSTALL_PREFIX={lmc_prefix}'))


@task.define(depends=[configure, get_cmake, calc_env])
async def build() -> None:
    cmake_bin = await task.result_of(get_cmake)
    res = await proc.run(
        [cmake_bin, '--build', build_dir.get()],
        on_output=_ninja_progress,
        env=await task.result_of(calc_env),
        print_output_on_finish='always',
    )
