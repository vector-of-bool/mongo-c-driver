import asyncio
from dataclasses import dataclass
import multiprocessing
import re
import string
import sys
from pathlib import Path
from typing import Awaitable, Iterable, Literal, NamedTuple, TypeVar

from dagon import fs, option, proc, task, ui

_THIS_FILE = Path(__file__).resolve()
_MONGOC_DIR = _THIS_FILE.parent.parent.parent

_LIBBSON_DOCS = _MONGOC_DIR / "src/libbson/doc"
_LIBMONGOC_DOCS = _MONGOC_DIR / "src/libmongoc/doc"
_DEFAULT_BUILD_DIR = _MONGOC_DIR / "_build"
T = TypeVar("T")

_OPT_DOCS_OUTPUT_BASE = option.add(
    "docs.output-dir",
    type=Path,
    doc="Base path for documentation output (Used to derive other output paths)",
    default=_DEFAULT_BUILD_DIR / "_docs",
)
_OPT_SPHINX_JOBS = option.add(
    "docs.sphinx.jobs",
    type=int,
    default=multiprocessing.cpu_count() + 2,
    doc="Set the number of parallel jobs to execute with Sphinx",
)


class DocTasks(NamedTuple):
    clean: task.Task[None]
    "The task that removes the built documentation files"
    build: task.Task[Path]
    "The task that builts the documentation output"


def _on_sphinx_output(out: proc.ProcessOutputItem):
    "Handle a line of Sphinx output"
    line = out.out.decode().rstrip()
    if out.kind == "error":
        ui.print(line, type=ui.MessageType.Error)
    else:
        ui.status(line)
    mat = re.search(r"(\d+)%", line)
    if not mat:
        return
    prog = int(mat[1])
    ui.progress(prog / 100)


async def sphinx_build(*, input_dir: Path, output_dir: Path, builder: Literal["html", "man"]) -> None:
    """
    Build a Sphinx documentation project

    :param input_dir: The directory that contains the documentation source. Should also contain conf.py
    :param output_dir: The directory in which to generate build output.
    :param builder: The Sphinx builder to use.
    :returns: The output directory.
    """
    jobs = _OPT_SPHINX_JOBS.get()
    cache_dir = output_dir.with_suffix(".doctrees")
    await proc.run(
        [
            sys.executable,
            "-m",
            "sphinx",
            ("-b", builder),
            ("-d", cache_dir),
            "-nWT",
            ("-j", jobs) if jobs else (),
            "--keep-going",
            input_dir,
            output_dir,
        ],
        on_output=_on_sphinx_output,
    )
    ui.print(f"Generated {builder} docs from [{input_dir}] in [{output_dir}]")


def create_sphinx_build_task(input_dir: Path, builder: Literal["html", "man"], basename: str) -> DocTasks:
    output_subdir = "man3" if builder == "man" else builder
    opt_output = option.add(
        f"docs.{basename}.{builder}.out",
        type=Path,
        default_factory=lambda: _OPT_DOCS_OUTPUT_BASE.get() / basename / output_subdir,
        doc=f"Output directory for {basename} “{builder}” documentation",
    )

    @task.define(name=f"docs.{basename}.{builder}.clean")
    async def clean() -> None:
        out_dir = opt_output.get()
        await asyncio.gather(
            fs.remove(out_dir, recurse=True, absent_ok=True),
            fs.remove(out_dir.with_suffix(".doctrees"), recurse=True, absent_ok=True),
        )

    @task.define(
        name=f"docs.{basename}.{builder}",
        order_only_depends=[clean],
        doc=f"Build “{builder}” documentation for {basename}",
    )
    async def build_docs() -> Path:
        out_dir = opt_output.get()
        await sphinx_build(input_dir=input_dir, output_dir=out_dir, builder=builder)
        return out_dir

    return DocTasks(clean, build_docs)


mongoc_html = create_sphinx_build_task(_LIBMONGOC_DOCS, "html", "libmongoc")
mongoc_man = create_sphinx_build_task(_LIBMONGOC_DOCS, "man", "libmongoc")
bson_html = create_sphinx_build_task(_LIBBSON_DOCS, "html", "libbson")
bson_man = create_sphinx_build_task(_LIBBSON_DOCS, "man", "libbson")
all_docs = (mongoc_html, bson_html, mongoc_man, bson_man)

task.gather("docs.man", [bson_man.build, mongoc_man.build])
task.gather("docs.html", [bson_html.build, mongoc_html.build])
task.gather("docs.clean", [d.clean for d in all_docs])


docs = task.gather("docs", [d.build for d in all_docs])


_AHA_TAG = "0.5.1"
"The Git tag of aha to clone and use for Man page rendering"
_AHA_PARELLEL = asyncio.Semaphore(multiprocessing.cpu_count() + 6)
"Limit to aha parallel rendering"


@dataclass
class _AhaProgress:
    nth: int
    total: int


def _aha_dir() -> Path:
    return _OPT_DOCS_OUTPUT_BASE.get() / f"_aha-{_AHA_TAG}"


@task.define()
async def __aha__get() -> Path:
    "Download and build an 'aha' executable (https://github.com/theZiz/aha.git)"
    aha_src = _aha_dir()
    if not aha_src.is_dir():
        aha_src.parent.mkdir(exist_ok=True, parents=True)
        ui.status("Cloning aha…")
        await proc.run(
            ["git", "clone", "--depth=1", "https://github.com/theZiz/aha.git", f"--branch={_AHA_TAG}", aha_src]
        )
    expect_bin = aha_src / "aha"
    await proc.run(["make", "-C", aha_src])
    assert expect_bin.is_file()
    return expect_bin


def _stdout_bytes(res: proc.ProcessResult) -> bytes:
    "Get the bytes of stdout from a completed subprocess"
    return b"".join(o.out for o in res.output if o.kind == "output")


async def _render_man_page(manfile: Path, aha_exe: Path, *, prog: _AhaProgress) -> tuple[Path, str]:
    """
    Generate HTML-formatted content of the given man page file.

    Returns the given path and the HTML string.
    """
    async with _AHA_PARELLEL:
        prog.nth += 1
        ui.progress(prog.nth / prog.total)
        ui.status(f"Processing page [{manfile}]")
        man_res = await proc.run(
            ["man", manfile],
            env={
                "MAN_KEEP_FORMATTING": "1",
                "COLUMNS": "80",
            },
        )
        man_stdout = _stdout_bytes(man_res)
        ul_res = await proc.run(
            ["ul", "-t", "xterm"],
            stdin=man_stdout,
        )
        ul_stdout = _stdout_bytes(ul_res)
        aha_res = await proc.run([aha_exe, "--no-header"], stdin=ul_stdout)
        return manfile, _stdout_bytes(aha_res).decode()


MAN_INDEX_HTML_TEMPLATE = string.Template(
    r"""
<html>
  <head>
    <meta charset="utf-8">
    <title>$PAGE_TITLE man pages</title>
    <style type="text/css">
      pre, div {
        margin: 2em;
        clear: both;
        float: left;
      }

      hr {
        width: 51em;
        font-family: monospace;
        clear: both;
        float: left;
      }
    </style>
  </head>
  <body>
    <pre>
        $BODY
    </pre>
  </body>
</html>
"""
)
ITEM_TEMPLATE = string.Template(
    r"""
    <div>$FILENAME</div>
    <hr>
    <pre>$CONTENT</pre>
    <hr>
    """
)


def _aio_gather(futs: Iterable[Awaitable[T]]) -> Awaitable[list[T]]:
    return asyncio.gather(*futs)


async def _create_index_html_for_man_pages(man_dir: Path, aha_bin: Path, title: str) -> str:
    threes = man_dir.glob("*.3")
    # Sort the .3 files by their filename, to produce a deterministic ordering:
    threes = sorted(threes)
    # Render each man page as HTML:
    prog = _AhaProgress(0, len(threes))
    ahas = [_render_man_page(page, aha_bin, prog=prog) for page in threes]
    pairs = await _aio_gather(ahas)

    divs = (ITEM_TEMPLATE.substitute(FILENAME=manfile.name, CONTENT=content) for manfile, content in pairs)
    trimmed = map(str.strip, divs)
    body = "".join(trimmed)
    return MAN_INDEX_HTML_TEMPLATE.substitute(PAGE_TITLE=title, BODY=body)


def create_man_index_html_task(libname: str, builder: task.Task[Path]):
    @task.define(
        name=f"docs.{libname}.man.index-html",
        depends=[builder, __aha__get],
        doc=f"Create an index.html with all man page contents for {libname}",
    )
    async def create_index_html():
        man_dir = await task.result_of(builder)
        aha_bin = await task.result_of(__aha__get)
        index_html_str = await _create_index_html_for_man_pages(man_dir, aha_bin, libname)
        man_dir.joinpath("index.html").write_text(index_html_str)

    return create_index_html


task.gather(
    "docs.man-index-htmls",
    [
        create_man_index_html_task("libbson", bson_man.build),
        create_man_index_html_task("libmongoc", mongoc_man.build),
    ],
    doc="Generate man pages and the index.html files for their contents",
)
