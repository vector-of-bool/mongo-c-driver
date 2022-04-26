import asyncio
import itertools
import json
import os
import platform
import re
import subprocess
import sys
import tempfile
from contextlib import AsyncExitStack, asynccontextmanager
from pathlib import Path
from typing import (IO, Any, AsyncContextManager, AsyncIterable, AsyncIterator, Mapping, NamedTuple, Optional)

import aiohttp
from dagon import ar, fs, option, persist, proc, task, ui

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


def _ninja_progress(message: proc.ProcessOutputItem) -> None:
    line = message.out.decode()
    ui.status(line)
    mat = PROG_RE.search(line)
    if not mat:
        return
    num, den = mat.groups()
    prog = int(num) / int(den)
    ui.progress(prog)


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
async def __get_cmake() -> Path:
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


class CMake(NamedTuple):
    cmake: Path
    ninja: Optional[Path]
    env: Optional[Mapping[str, str]]

    async def configure(self,
                        src_dir: Path,
                        bin_dir: Path,
                        *,
                        cmake_flags: proc.CommandLine = (),
                        on_output: proc.OutputMode = 'print') -> None:
        cmd = [
            self.cmake,
            f'-S{src_dir}',
            f'-B{bin_dir}',
            '-GNinja Multi-Config',
            f'-DCMAKE_MAKE_PROGRAM={self.ninja}',
            cmake_flags,
        ]
        try:
            await proc.run(cmd, on_output=on_output, env=self.env)
        except subprocess.CalledProcessError as e:
            if b'No CMAKE_C_COMPILER could be found' in e.stderr and os.name == 'nt':
                ui.print('Note: No C compiler could be found. Did you mean to set a "vs-version" option?',
                         type=ui.MessageType.Warning)
            raise

    async def build(self,
                    bin_dir: Path,
                    *,
                    target: Optional[str] = None,
                    install: bool = False,
                    config: str = 'Debug',
                    print_on_finish: proc._PrintOnFinishArg = 'on-fail') -> None:
        build_cmd: proc.CommandLine = [
            self.cmake,
            ('--build', bin_dir),
            ('--config', config),
        ]
        if target:
            build_cmd.append(('--target', target))
        await proc.run(build_cmd, env=self.env, on_output=_ninja_progress, print_output_on_finish=print_on_finish)
        if install:
            install_cmd: proc.CommandLine = [
                self.cmake,
                f'-DCMAKE_INSTALL_CONFIG_NAME={config}',
                '-P',
                bin_dir / 'cmake_install.cmake',
            ]
            await proc.run(install_cmd)


@task.define(depends=[__get_cmake, get_ninja, calc_env])
async def get_cmake() -> CMake:
    return CMake(await task.result_of(__get_cmake), await task.result_of(get_ninja), await task.result_of(calc_env))


@task.define(order_only_depends=[clean])
async def dl_libmongocrypt() -> Path:
    tag = libmongocrypt_version.get()
    tar_dest = build_dir.get() / f'libmongocrypt-{tag}.tar.gz'
    await _download(f'https://github.com/mongodb/libmongocrypt/archive/refs/heads/master.tar.gz', tar_dest)
    root = build_dir.get() / f'libmongocrypt-{tag}'
    stamp = root / 'extracted.stamp'
    if stamp.is_file():
        return root
    await fs.remove(root, recurse=True, absent_ok=True)
    await ar.expand(tar_dest, destination=root, on_extract=_on_extract, strip_components=1)
    stamp.write_bytes(b'')
    return root


@task.define(order_only_depends=[clean], depends=[get_cmake])
async def build_install_libbson_tmp() -> Path:
    src_dir = HERE
    tmp_dir = src_dir / '_build/_libbson-only'
    prefix = src_dir / '_build/_libbson-install'
    cmake = await task.result_of(get_cmake)
    await cmake.configure(src_dir,
                          tmp_dir,
                          cmake_flags=('-DENABLE_MONGOC=OFF', f'-DCMAKE_INSTALL_PREFIX={prefix}'),
                          on_output='status')
    await cmake.build(tmp_dir, install=True)
    return prefix


@task.define(order_only_depends=[clean], depends=[get_cmake, dl_libmongocrypt, build_install_libbson_tmp])
async def build_libmongocrypt() -> Path:
    src_dir = await task.result_of(dl_libmongocrypt)
    bin_dir = src_dir / '_build/_libmongocrypt'
    prefix = src_dir / '_build/_libmongocrypt-install'
    stamp = prefix / f'{libmongocrypt_version.get()}.install.stamp'
    # if stamp.is_file():
    #     return prefix
    cmake = await task.result_of(get_cmake)
    libbson_root = await task.result_of(build_install_libbson_tmp)
    await cmake.configure(src_dir,
                          bin_dir,
                          cmake_flags=(f'-DCMAKE_INSTALL_PREFIX={prefix}', f'-DCMAKE_PREFIX_PATH={libbson_root}'),
                          on_output='status')
    await cmake.build(bin_dir, install=True)
    stamp.write_bytes(b'')
    return prefix


@task.define(order_only_depends=[clean], depends=[get_cmake, build_libmongocrypt])
async def configure() -> None:
    lmc_prefix = await task.result_of(build_libmongocrypt)
    cmake = await task.result_of(get_cmake)
    await cmake.configure(HERE, build_dir.get(), cmake_flags=(f'-DCMAKE_INSTALL_PREFIX={lmc_prefix}'))


@task.define(depends=[configure, get_cmake, calc_env])
async def build() -> None:
    cmake = await task.result_of(get_cmake)
    await cmake.build(build_dir.get(), print_on_finish='always')


@task.define()
async def __evg_tools() -> Path:
    clone_dir = build_dir.get() / '_evergreen-driver-tools'
    if clone_dir.joinpath('.git').is_dir():
        return clone_dir
    await proc.run(
        ['git', 'clone', 'https://github.com/mongodb-labs/drivers-evergreen-tools', '--depth=1', '--quiet', clone_dir])
    return clone_dir


test_pattern = option.add('test.pattern', str, doc='The globbing pattern to select a subset of tests to run')
test_debug = option.add('test.debug', bool, default=False, doc='Enable test debug output')
test_env = option.add('test.env', Path, default=None, doc='A JSON file of environment variables to load for the test')


def load_secrets(fpath: Path) -> Mapping[str, str]:
    secrets = json.loads(fpath.read_bytes())
    varmap = {
        # AWS
        'AWS_SECRET_ACCESS_KEY': secrets['aws']['secretAccessKey'],
        'AWS_ACCESS_KEY_ID': secrets['aws']['accessKeyId'],
        # Azure
        'AZURE_TENANT_ID': secrets['azure']['tenantId'],
        'AZURE_CLIENT_ID': secrets['azure']['clientId'],
        'AZURE_CLIENT_SECRET': secrets['azure']['clientSecret'],
        # GCP
        'GCP_EMAIL': secrets['gcp']['email'],
        'GCP_PRIVATEKEY': secrets['gcp']['privateKey'],
    }
    return varmap


secrets_json = option.add('cse.secrets.json',
                          Path,
                          default=None,
                          doc='A JSON file containing secrets for client-side encryption tests')

mongocryptd_path = option.add('test.mongocryptd', Path, default=None, doc='A mongocryptd executable to use for testing')


@asynccontextmanager
async def _tmp_process(cmd: proc.CommandLine) -> AsyncIterator[proc.RunningProcess]:
    cmd = proc.plain_commandline(cmd)
    cancel = proc.CancellationToken()
    ui.print(f'Start: {cmd}')
    child = await proc.spawn(cmd, cancel=cancel, print_output_on_finish='never')
    try:
        yield child
    finally:
        cancel.cancel()
        try:
            res = await child.result
        except asyncio.CancelledError:
            pass


def mongocryptd_process() -> AsyncContextManager[proc.RunningProcess]:
    md = mongocryptd_path.get()
    if not md:
        md = _which('mongocryptd')
    if not md:
        raise RuntimeError('No [mongocrypt] executable provided or available on PATH.')
    return _tmp_process([md, f'--logpath={build_dir.get()/"test-mongocryptd.log"}'])


def _kmip_server(evg: Path) -> AsyncContextManager[proc.RunningProcess]:
    return _tmp_process([sys.executable, '-u', evg / '.evergreen/csfle/kms_kmip_server.py'])


def tmp_kms(evg: Path, cert: str, port: int,
            more_args: proc.CommandLine = ()) -> AsyncContextManager[proc.RunningProcess]:
    return _tmp_process([
        sys.executable,
        '-u',
        evg / '.evergreen/csfle/kms_http_server.py',
        ('--ca_file', evg / '.evergreen/x509gen/ca.pem'),
        ('--cert_file', evg / f'.evergreen/x509gen/{cert}.pem'),
        ('--port', port),
        more_args,
    ])


@task.define(order_only_depends=[build], depends=[build_libmongocrypt, __evg_tools])
async def test() -> None:
    tester = build_dir.get() / 'src/libmongoc/Debug/test-libmongoc'
    env = os.environ.copy()
    if os.name == 'nt':
        tester = tester.with_suffix('.exe')
        lmc_install_dir = await task.result_of(build_libmongocrypt)
        env['PATH'] += f';{lmc_install_dir/"bin"}'

    evg_tools_dir = await task.result_of(__evg_tools)

    cmd: proc.CommandLine = [tester, '--no-fork']
    pat = test_pattern.get(default=None)
    if pat:
        cmd.extend(('-l', pat))
    if test_debug.get():
        cmd.append('-d')
    sj = secrets_json.get()
    if sj:
        vars = load_secrets(sj)
        env.update({f'MONGOC_TEST_{key}': val for key, val in vars.items()})
    te = test_env.get()
    if te:
        env_data = json.loads(te.read_text("utf-8"))
        env.update(env_data)
    env.update({
        'MONGOC_TEST_CSFLE_TLS_CA_FILE': str(evg_tools_dir / '.evergreen/x509gen/ca.pem'),
        'MONGOC_TEST_CSFLE_TLS_CERTIFICATE_KEY_FILE': str(evg_tools_dir / '.evergreen/x509gen/client.pem'),
        'MONGOC_TEST_MONGOCRYPTD_BYPASS_SPAWN': 'on',
    })
    async with AsyncExitStack() as st:
        ui.status(f'Spawning temporary servers for CSE')
        await st.enter_async_context(mongocryptd_process())
        await st.enter_async_context(_kmip_server(evg_tools_dir))
        await st.enter_async_context(tmp_kms(evg_tools_dir, 'server', 7999))
        await st.enter_async_context(tmp_kms(evg_tools_dir, 'expired', 8000))
        await st.enter_async_context(tmp_kms(evg_tools_dir, 'wrong-host', 8001))
        await st.enter_async_context(tmp_kms(evg_tools_dir, 'server', 8002, ['--require_client_cert']))
        ui.status('Running tests...')
        await proc.run(cmd, cwd=HERE, on_output='print', env=env)
        ui.status('Shutting down temporary servers...')
