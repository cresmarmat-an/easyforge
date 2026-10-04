"""Builds the documentation site from the repository.

    python .github/site/build.py [--output _site] [--strict] [--site-url URL]

Everything the site shows comes from the repository, so a change to the
repository is all it takes to change the site:

- docs/: each .md file is a page at <folder>/<name>/, titled by its first
  "# " heading. docs/nav.json sets the order and titles of sections and pages;
  anything it does not list follows what it lists, so a new page only needs a
  new file.
- docs/index.md is the home page. Its first paragraph is the line under the
  name, the rest of its opening sits beside the logo, and each "## " section
  follows in order: a section with code becomes the example, a table whose
  first column links to pages becomes cards, a list of links becomes cards,
  and anything else is shown as written.
- docs/logo.png is the logo, the icons, and the picture shown with shared links.
- CMakeLists.txt gives the name, version, and description; LICENSE the license,
  author, and year; README.md the examples link; CHANGELOG.md the changelog page
  and the latest release on the home page. The examples card's description is
  the opening paragraph of the examples repository's README, read from GitHub.
- The repository's address comes from GitHub Actions, or from git's origin.

Broken links and missing headings are printed as warnings. With --strict they
also fail the build. The images need Pillow (pip install -r
.github/site/requirements.txt).
"""

import argparse
import hashlib
import html
import json
import os
import posixpath
import re
import shutil
import subprocess
import sys
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

SITE_DIRECTORY = Path(__file__).resolve().parent
REPOSITORY = SITE_DIRECTORY.parents[1]
DOCS = REPOSITORY / "docs"
TEMPLATES = SITE_DIRECTORY / "templates"
LOGO = DOCS / "logo.png"
# The folder in the built site that holds its styles, scripts, images, and
# search index. A folder in docs/ with this name would collide with it.
FILES = "site"


def fail(message):
    sys.exit(f"build.py: {message}")


def read(path):
    return Path(path).read_text(encoding="utf-8").replace("\r\n", "\n")


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode("utf-8"))


# What the repository says about itself ----------------------------------------

@dataclass
class Project:
    name: str
    version: str
    description: str
    license_name: str
    copyright_year: str
    author: str
    repository: str
    branch: str
    site_url: str
    author_url: str
    examples: str


def git(*arguments):
    try:
        result = subprocess.run(["git", "-C", str(REPOSITORY), *arguments], capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return result.stdout.strip() if result.returncode == 0 else ""


def repository_address():
    """https://github.com/owner/name, from GitHub Actions or from git's origin."""
    server, repository = os.environ.get("GITHUB_SERVER_URL"), os.environ.get("GITHUB_REPOSITORY")
    if server and repository:
        return f"{server}/{repository}"
    origin = git("remote", "get-url", "origin")
    match = re.match(r"^(?:https?://|ssh://)?(?:[^@/]+@)?([^/:]+)[:/](.+?)(?:\.git)?/?$", origin)
    if not match:
        fail("cannot tell the repository's address: set GITHUB_REPOSITORY or add a git remote named origin")
    return f"https://{match.group(1)}/{match.group(2)}"


def branch_name():
    if os.environ.get("GITHUB_REF_TYPE") == "branch" and os.environ.get("GITHUB_REF_NAME"):
        return os.environ["GITHUB_REF_NAME"]
    branch = git("rev-parse", "--abbrev-ref", "HEAD")
    return branch if branch and branch != "HEAD" else "main"


def pages_address(repository):
    """Where GitHub Pages serves a repository's site."""
    owner, _, name = repository.split("://", 1)[1].split("/", 1)[1].partition("/")
    if name.lower() == f"{owner}.github.io".lower():
        return f"https://{owner.lower()}.github.io/"
    return f"https://{owner.lower()}.github.io/{name}/"


def load_project(site_url):
    cmake = read(REPOSITORY / "CMakeLists.txt")
    project = re.search(r"project\(\s*([\w-]+)(.*?)\)", cmake, re.S)
    if not project:
        fail("CMakeLists.txt has no project() call")
    name, details = project.group(1), project.group(2)
    number = re.search(r"\bVERSION\s+([\d.]+)", details)
    label = re.search(r"set\(\s*" + re.escape(name.upper()) + r'_VERSION_LABEL\s+"([^"]*)"\s*\)', cmake)
    description = re.search(r'\bDESCRIPTION\s+"([^"]*)"', details)
    version = (number.group(1) if number else "") + (f"-{label.group(1)}" if label and label.group(1) else "")

    license_text = read(REPOSITORY / "LICENSE") if (REPOSITORY / "LICENSE").exists() else ""
    license_lines = [line.strip() for line in license_text.split("\n") if line.strip()]
    copyright = re.search(r"Copyright\s+(?:\(c\)|©)\s*([\d\s,-]+?)\s+(.+)", license_text, re.I)

    repository = repository_address()
    owner = repository.split("://", 1)[1].split("/")[1]
    readme = read(REPOSITORY / "README.md") if (REPOSITORY / "README.md").exists() else ""
    examples = re.search(r"\[\**Examples\**\]\((https?://[^)\s]+)\)", readme, re.I)

    site_url = site_url or pages_address(repository)
    return Project(
        name=name,
        version=version,
        description=description.group(1) if description else "",
        license_name=license_lines[0] if license_lines else "its license",
        copyright_year=copyright.group(1).strip() if copyright else "",
        author=copyright.group(2).strip().rstrip(".") if copyright else owner,
        repository=repository,
        branch=branch_name(),
        site_url=site_url if site_url.endswith("/") else site_url + "/",
        author_url=f"https://{owner.lower()}.github.io/",
        examples=examples.group(1) if examples else f"{repository}-examples",
    )


# Pages -------------------------------------------------------------------------

@dataclass
class Page:
    path: str  # relative to docs/, such as "core/math.md"
    text: str
    source: str  # relative to the repository, for the link to GitHub
    editable: bool = True
    title: str = ""
    anchors: set = field(default_factory=set)

    @property
    def slug(self):
        return "" if self.path == "index.md" else self.path[:-3]


class Site:
    def __init__(self, project, strict):
        self.project = project
        self.strict = strict
        self.warnings = []
        self.pages = {}
        self.sections = []
        self.files = set()

    def warn(self, message, source=None):
        self.warnings.append(message)
        if os.environ.get("GITHUB_ACTIONS") == "true":
            where = f" file={source}" if source else ""
            print(f"::warning{where}::{message}")
        else:
            print(f"warning: {message}", file=sys.stderr)


def title_from_name(name):
    words = name.replace("-", " ").replace("_", " ").strip()
    return words[:1].upper() + words[1:]


def plain_text(markdown):
    """The readable text of inline Markdown, used for titles and anchors."""
    text = re.sub(r"\\(.)", r"\1", markdown)
    text = re.sub(r"!\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"`+([^`]*)`+", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"(\*\*|__|\*|_)(\S.*?\S|\S)\1", r"\2", text)
    return text.strip()


def slugify(text):
    text = text.strip().lower()
    text = re.sub(r"[^\w\s-]", "", text)
    text = re.sub(r"\s+", "-", text)
    return text.strip("-") or "section"


class Slugger:
    """Gives each heading a unique anchor, adding -1, -2, ... to repeats, as GitHub does."""

    def __init__(self):
        self.seen = {}

    def anchor(self, text):
        base = slugify(text)
        count = self.seen.get(base, 0)
        self.seen[base] = count + 1
        return base if count == 0 else f"{base}-{count}"


FENCE = re.compile(r"^(\s*)(`{3,}|~{3,})\s*([\w+#.-]*)\s*$")
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")


def headings_of(text):
    """(level, inline markdown) for every heading outside code blocks."""
    found = []
    fence = None
    for line in text.split("\n"):
        match = FENCE.match(line)
        if match:
            marker = match.group(2)
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
            continue
        if fence is None:
            heading = HEADING.match(line)
            if heading:
                found.append((len(heading.group(1)), heading.group(2)))
    return found


def strip_front_matter(text):
    if text.startswith("---\n"):
        end = text.find("\n---", 4)
        if end != -1:
            return text[text.find("\n", end + 1) + 1:]
    return text


# Syntax highlighting -----------------------------------------------------------

CPP_KEYWORDS = (
    "alignas auto bool break case catch char class concept const consteval constexpr continue decltype "
    "default delete do double else enum explicit export extern false final float for friend if inline int "
    "long mutable namespace new noexcept nullptr operator override private protected public requires return "
    "short signed sizeof static static_assert struct switch template this throw true try typedef typename "
    "union unsigned using virtual void volatile while"
)
SCRIPT_KEYWORDS = (
    "function returns then end variable constant if else while for in to return break continue and or not "
    "true false nothing type import try catch spawn wait yield of"
)
SCRIPT_TYPES = "number string boolean list table any"
SHADER_TYPES = SCRIPT_TYPES + " color vector2 vector3 vector4 matrix4 texture"
NUMBER = r"\b0[xX][0-9a-fA-F]+\b|\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?[fFuUlL]*\b"


def words(names):
    return r"\b(?:" + "|".join(names.split()) + r")\b"


GRAMMARS = {
    "cpp": [
        ("comment", r"//[^\n]*|/\*[\s\S]*?\*/"),
        ("preprocessor", r"^[ \t]*#[ \t]*\w+[^\n]*"),
        ("string", r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\''),
        ("number", NUMBER),
        ("keyword", words(CPP_KEYWORDS)),
    ],
    "script": [
        ("comment", r"--\[\[[\s\S]*?\]\]|--[^\n]*"),
        ("string", r'"(?:\\.|[^"\\\n])*"'),
        ("number", NUMBER + r"|#[0-9a-fA-F]{3,8}\b"),
        ("keyword", words(SCRIPT_KEYWORDS)),
        ("type", words(SCRIPT_TYPES)),
    ],
    "shader": [
        ("comment", r"--\[\[[\s\S]*?\]\]|--[^\n]*"),
        ("string", r'"(?:\\.|[^"\\\n])*"'),
        ("number", NUMBER + r"|#[0-9a-fA-F]{3,8}\b"),
        ("keyword", words(SCRIPT_KEYWORDS + " value")),
        ("type", words(SHADER_TYPES)),
    ],
    "cmake": [
        ("comment", r"#[^\n]*"),
        ("string", r'"(?:\\.|[^"\\])*"'),
        ("variable", r"\$\{[^}\n]*\}"),
        ("keyword", r"\b[A-Za-z_]\w*(?=\()"),
        ("constant", r"\b[A-Z][A-Z0-9_]{1,}\b"),
    ],
    "shell": [
        ("comment", r"(?<![\w$])#[^\n]*"),
        ("string", r'"(?:\\.|[^"\\])*"|\'[^\'\n]*\''),
        ("keyword", r"^[ \t]*[\w.-]+"),
        ("constant", r"(?<=\s)--?[\w-]+"),
    ],
    "json": [
        ("property", r'"(?:\\.|[^"\\])*"(?=\s*:)'),
        ("string", r'"(?:\\.|[^"\\])*"'),
        ("number", r"-?\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b"),
        ("keyword", r"\b(?:true|false|null)\b"),
    ],
}

LANGUAGES = {
    "c++": "cpp", "cpp": "cpp", "cxx": "cpp", "h": "cpp", "hpp": "cpp",
    "script": "script", "shader": "shader",
    "cmake": "cmake",
    "bash": "shell", "sh": "shell", "shell": "shell", "powershell": "shell", "ps1": "shell", "console": "shell",
    "json": "json",
}

# The name shown on a code block, by the language its fence names.
LANGUAGE_NAMES = {
    "c++": "C++", "cpp": "C++", "cxx": "C++", "h": "C++", "hpp": "C++", "c": "C",
    "script": "Script", "shader": "Shader", "cmake": "CMake",
    "bash": "Shell", "sh": "Shell", "shell": "Shell", "console": "Shell",
    "powershell": "PowerShell", "ps1": "PowerShell", "json": "JSON", "text": "", "": "",
}

COMPILED_GRAMMARS = {}


def highlight(code, language):
    grammar_name = LANGUAGES.get(language.lower())
    if grammar_name is None:
        return html.escape(code)
    if grammar_name not in COMPILED_GRAMMARS:
        pattern = "|".join(f"(?P<g{index}>{rule})" for index, (_, rule) in enumerate(GRAMMARS[grammar_name]))
        COMPILED_GRAMMARS[grammar_name] = re.compile(pattern, re.MULTILINE)
    compiled = COMPILED_GRAMMARS[grammar_name]
    rules = GRAMMARS[grammar_name]

    parts = []
    position = 0
    for match in compiled.finditer(code):
        if match.start() == match.end():
            continue
        parts.append(html.escape(code[position:match.start()]))
        kind = rules[int(match.lastgroup[1:])][0]
        parts.append(f'<span class="token-{kind}">{html.escape(match.group())}</span>')
        position = match.end()
    parts.append(html.escape(code[position:]))
    return "".join(parts)


def code_block_html(code, language):
    name = LANGUAGE_NAMES.get(language.lower(), language.upper())
    label = f' data-language="{html.escape(name)}"' if name else ""
    return f'<div class="highlight"{label}><pre><code>{highlight(code, language)}</code></pre></div>'


# Markdown ----------------------------------------------------------------------

RULE = re.compile(r"^\s{0,3}([-*_])(\s*\1){2,}\s*$")
TABLE_SEPARATOR = re.compile(r"^\s*\|?\s*:?-+:?\s*(\|\s*:?-+:?\s*)*\|?\s*$")
LIST_ITEM = re.compile(r"^(\s*)([-*+]|\d{1,9}[.)])(\s+|$)")
QUOTE = re.compile(r"^\s{0,3}>\s?(.*)$")
CALLOUT = re.compile(r"^\[!(\w+)\][+-]?\s*(.*)$")
HTML_BLOCK = re.compile(
    r"^\s*</?(?:div|p|table|details|summary|figure|img|picture|section|aside|pre|ul|ol|h[1-6]|hr|br)\b", re.I)
EXTERNAL = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")

CALLOUT_TITLES = {
    "note": "Note", "tip": "Tip", "important": "Important", "warning": "Warning", "caution": "Caution",
}


def split_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|") and not line.endswith("\\|"):
        line = line[:-1]
    cells = []
    current = []
    in_code = False
    position = 0
    while position < len(line):
        character = line[position]
        if character == "\\" and position + 1 < len(line) and line[position + 1] == "|":
            current.append("|")
            position += 2
            continue
        if character == "`":
            in_code = not in_code
        if character == "|" and not in_code:
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(character)
        position += 1
    cells.append("".join(current).strip())
    return cells


class MarkdownRenderer:
    def __init__(self, site, page):
        self.site = site
        self.page = page
        self.slugger = Slugger()
        self.outline = []

    def render(self, text):
        lines = text.replace("\r\n", "\n").replace("\t", "    ").split("\n")
        return self.blocks(lines)

    # Blocks

    def starts_block(self, line):
        return bool(
            FENCE.match(line) or HEADING.match(line) or RULE.match(line) or QUOTE.match(line)
            or LIST_ITEM.match(line) or HTML_BLOCK.match(line))

    def blocks(self, lines, tight=False):
        output = []
        index = 0
        while index < len(lines):
            line = lines[index]
            if not line.strip():
                index += 1
                continue

            fence = FENCE.match(line)
            if fence:
                index = self.code_block(lines, index, fence, output)
                continue

            heading = HEADING.match(line)
            if heading:
                output.append(self.heading(len(heading.group(1)), heading.group(2)))
                index += 1
                continue

            if RULE.match(line):
                output.append("<hr>")
                index += 1
                continue

            if QUOTE.match(line):
                index = self.quote(lines, index, output)
                continue

            if LIST_ITEM.match(line) and LIST_ITEM.match(line).group(3):
                index = self.list_block(lines, index, output)
                continue

            if "|" in line and index + 1 < len(lines) and TABLE_SEPARATOR.match(lines[index + 1]):
                index = self.table(lines, index, output)
                continue

            if HTML_BLOCK.match(line):
                start = index
                while index < len(lines) and lines[index].strip():
                    index += 1
                output.append("\n".join(lines[start:index]))
                continue

            index = self.paragraph(lines, index, output, tight)
        return "\n".join(output)

    def paragraph(self, lines, index, output, tight):
        collected = []
        while index < len(lines) and lines[index].strip():
            if collected and self.starts_block(lines[index]):
                break
            collected.append(lines[index].strip())
            index += 1
        text = self.inline(" ".join(collected))
        output.append(text if tight else f"<p>{text}</p>")
        return index

    def code_block(self, lines, index, fence, output):
        indent = len(fence.group(1))
        marker = fence.group(2)
        language = fence.group(3)
        index += 1
        code = []
        while index < len(lines):
            closing = FENCE.match(lines[index])
            if closing and closing.group(2)[0] == marker[0] and len(closing.group(2)) >= len(marker) \
                    and not closing.group(3):
                index += 1
                break
            line = lines[index]
            code.append(line[indent:] if line[:indent].strip() == "" else line.lstrip())
            index += 1
        output.append(code_block_html("\n".join(code), language))
        return index

    def heading(self, level, markdown):
        anchor = self.slugger.anchor(plain_text(markdown))
        content = self.inline(markdown)
        if level in (2, 3):
            self.outline.append((level, html.unescape(re.sub(r"<[^>]+>", "", content)), anchor))
        return f'<h{level} id="{anchor}">{content}</h{level}>'

    def quote(self, lines, index, output):
        inner = []
        while index < len(lines):
            match = QUOTE.match(lines[index])
            if not match:
                break
            inner.append(match.group(1))
            index += 1

        callout = CALLOUT.match(inner[0].strip()) if inner else None
        if callout:
            kind = callout.group(1).lower()
            title = callout.group(2).strip() or CALLOUT_TITLES.get(kind, title_from_name(kind))
            body = self.blocks(inner[1:])
            output.append(
                f'<div class="callout callout-{html.escape(kind)}">'
                f'<p class="callout-title">{self.inline(title)}</p>{body}</div>')
        else:
            output.append(f"<blockquote>{self.blocks(inner)}</blockquote>")
        return index

    def list_block(self, lines, index, output):
        first = LIST_ITEM.match(lines[index])
        base_indent = len(first.group(1))
        ordered = first.group(2)[0].isdigit()
        start = int(first.group(2)[:-1]) if ordered else 1

        def same_list(match):
            return match and len(match.group(1)) == base_indent and match.group(2)[0].isdigit() == ordered

        items = []
        loose = False
        while index < len(lines):
            line = lines[index]
            match = LIST_ITEM.match(line)
            if same_list(match):
                content_indent = len(match.group(1)) + len(match.group(2)) + max(1, len(match.group(3)))
                items.append({"indent": content_indent, "lines": [line[content_indent:]]})
                index += 1
                continue

            if not line.strip():
                following = index + 1
                while following < len(lines) and not lines[following].strip():
                    following += 1
                if following >= len(lines):
                    break
                next_line = lines[following]
                next_indent = len(next_line) - len(next_line.lstrip())
                if same_list(LIST_ITEM.match(next_line)) or next_indent >= items[-1]["indent"]:
                    loose = True
                    items[-1]["lines"].append("")
                    index += 1
                    continue
                break

            indent = len(line) - len(line.lstrip())
            if indent >= items[-1]["indent"]:
                items[-1]["lines"].append(line[items[-1]["indent"]:])
            elif indent > base_indent or not self.starts_block(line):
                items[-1]["lines"].append(line.strip())
            else:
                break
            index += 1

        tag = "ol" if ordered else "ul"
        start_attribute = f' start="{start}"' if ordered and start != 1 else ""
        rendered = "".join(f"<li>{self.blocks(item['lines'], tight=not loose)}</li>" for item in items)
        output.append(f"<{tag}{start_attribute}>{rendered}</{tag}>")
        return index

    def table(self, lines, index, output):
        header = split_row(lines[index])
        alignments = []
        for cell in split_row(lines[index + 1]):
            left, right = cell.startswith(":"), cell.endswith(":")
            alignments.append("center" if left and right else "right" if right else "left" if left else None)
        index += 2

        rows = []
        while index < len(lines) and lines[index].strip() and "|" in lines[index]:
            rows.append(split_row(lines[index]))
            index += 1

        def cell(tag, text, column):
            alignment = alignments[column] if column < len(alignments) else None
            style = f' style="text-align: {alignment}"' if alignment else ""
            return f"<{tag}{style}>{self.inline(text)}</{tag}>"

        head = "".join(cell("th", text, column) for column, text in enumerate(header))
        head_html = f"<thead><tr>{head}</tr></thead>" if any(text.strip() for text in header) else ""
        body = "".join(
            "<tr>" + "".join(cell("td", row[column] if column < len(row) else "", column)
                             for column in range(len(header))) + "</tr>"
            for row in rows)
        output.append(f'<div class="table-wrap"><table>{head_html}<tbody>{body}</tbody></table></div>')
        return index

    # Inline text

    def inline(self, text):
        store = []
        result = self.inline_parts(text, store)
        while "\x00" in result:
            result = re.sub(r"\x00(\d+)\x00", lambda match: store[int(match.group(1))], result)
        return result

    def inline_parts(self, text, store):
        def hold(value):
            store.append(value)
            return f"\x00{len(store) - 1}\x00"

        text = re.sub(r"\\([\\`*_{}\[\]()#+\-.!|<>~])", lambda match: hold(html.escape(match.group(1))), text)
        text = re.sub(r"(`+)(.+?)\1", lambda match: hold(f"<code>{html.escape(match.group(2).strip())}</code>"), text)
        text = re.sub(r"<(https?://[^>\s]+)>",
                      lambda match: hold(f'<a href="{html.escape(match.group(1))}">{html.escape(match.group(1))}</a>'),
                      text)
        text = re.sub(r"<(br|kbd|/kbd)\s*/?>", lambda match: hold(f"<{match.group(1)}>"), text)

        def image(match):
            source = self.rewrite_link(match.group(2))
            title = f' title="{html.escape(match.group(3))}"' if match.group(3) else ""
            return hold(f'<img src="{html.escape(source)}" alt="{html.escape(match.group(1))}"{title} loading="lazy">')

        text = re.sub(r'!\[([^\]]*)\]\(([^)\s]+)(?:\s+"([^"]*)")?\)', image, text)

        def link(match):
            label = self.inline_parts(match.group(1), store)
            target = self.rewrite_link(match.group(2))
            attributes = ' rel="noopener"' if EXTERNAL.match(target) else ""
            return hold(f'<a href="{html.escape(target)}"{attributes}>{label}</a>')

        text = re.sub(r'\[((?:[^\[\]]|\[[^\]]*\])+)\]\(([^)\s]+)(?:\s+"([^"]*)")?\)', link, text)

        text = html.escape(text, quote=False)
        text = re.sub(r"\*\*(?=\S)(.+?)(?<=\S)\*\*", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<![\w_])__(?=\S)(.+?)(?<=\S)__(?![\w_])", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<![\w*])\*(?=\S)(.+?)(?<=\S)\*(?![\w*])", r"<em>\1</em>", text)
        text = re.sub(r"(?<![\w_])_(?=\S)(.+?)(?<=\S)_(?![\w_])", r"<em>\1</em>", text)
        return text

    def rewrite_link(self, target):
        """A link as the Markdown wrote it, for reading on GitHub, turned into
        the address it has on this site."""
        if EXTERNAL.match(target) or target.startswith("//"):
            return target

        path, _, anchor = target.partition("#")
        pages = self.site.pages
        where = self.page.slug or "."
        source = self.page.source
        if not path:
            if anchor and anchor not in self.page.anchors:
                self.site.warn(f"{self.page.path}: link to missing heading #{anchor}", source)
            return target

        # Where the link points in the repository, seen from the file that has it.
        inside = posixpath.normpath(posixpath.join(posixpath.dirname(source), path))
        if inside == ".." or inside.startswith("../"):
            self.site.warn(f"{self.page.path}: link leaves the repository: {path}", source)
            return target

        if inside.startswith("docs/") and inside[5:] in pages:
            page = pages[inside[5:]]
            if anchor and anchor not in page.anchors:
                self.site.warn(f"{self.page.path}: link to missing heading {path}#{anchor}", source)
            return page_address(where, page.slug, anchor)
        for page in pages.values():
            if page.source == inside:
                if anchor and anchor not in page.anchors:
                    self.site.warn(f"{self.page.path}: link to missing heading {path}#{anchor}", source)
                return page_address(where, page.slug, anchor)

        if not (REPOSITORY / inside).exists():
            self.site.warn(f"{self.page.path}: link to missing file {path}", source)
            return target
        if inside.startswith("docs/") and (REPOSITORY / inside).is_file():
            # Images and other files beside the pages are copied into the site.
            self.site.files.add(inside[5:])
            return posixpath.relpath(inside[5:], where) + (f"#{anchor}" if anchor else "")
        # Anything else in the repository is shown on GitHub.
        kind = "tree" if (REPOSITORY / inside).is_dir() else "blob"
        return f"{self.site.project.repository}/{kind}/{self.site.project.branch}/{inside}" + \
            (f"#{anchor}" if anchor else "")


def page_address(from_slug, to_slug, anchor=""):
    """A relative address from one page's folder to another's ("" is the home page)."""
    relative = posixpath.relpath(to_slug or ".", from_slug or ".")
    relative = "./" if relative == "." else relative + "/"
    return relative + (f"#{anchor}" if anchor else "")


# Finding the pages -------------------------------------------------------------

def load_pages(site):
    project = site.project
    for file in sorted(DOCS.rglob("*.md")):
        relative = file.relative_to(DOCS).as_posix()
        site.pages[relative] = Page(relative, strip_front_matter(read(file)), f"docs/{relative}")

    changelog = REPOSITORY / "CHANGELOG.md"
    if changelog.exists():
        site.pages["reference/changelog.md"] = Page("reference/changelog.md", read(changelog), "CHANGELOG.md",
                                                    editable=False)

    license_file = REPOSITORY / "LICENSE"
    if license_file.exists():
        text = (f"# License\n\n{project.name} is released under the {project.license_name}. A program that uses it "
                f"must keep this notice with its copies of {project.name}.\n\n```text\n"
                + read(license_file).rstrip() + "\n```\n")
        site.pages["reference/license.md"] = Page("reference/license.md", text, "LICENSE", editable=False)

    for page in site.pages.values():
        headings = headings_of(page.text)
        titles = [markdown for level, markdown in headings if level == 1]
        name = posixpath.splitext(posixpath.basename(page.path))[0]
        page.title = plain_text(titles[0]) if titles else title_from_name(name)
        slugger = Slugger()
        page.anchors = {slugger.anchor(plain_text(markdown)) for _, markdown in headings}


def load_navigation(site):
    """Sections in order, as (title, [(page path, label in the navigation)]).

    A section in nav.json either names a folder, whose pages it lists by name
    and whose unlisted pages follow, or lists pages from anywhere by their path
    without ".md". A page can be given as {"page": ..., "title": ...} to show a
    shorter label in the navigation than its own title.
    """
    navigation_file = DOCS / "nav.json"
    config = {}
    if navigation_file.exists():
        try:
            config = json.loads(read(navigation_file))
        except json.JSONDecodeError as error:
            site.warn(f"nav.json is not valid JSON ({error}); pages are in alphabetical order", "docs/nav.json")

    def entry_of(item, folder):
        name, label = (item["page"], item.get("title")) if isinstance(item, dict) else (item, None)
        path = f"{folder}/{name}.md" if folder else f"{name}.md"
        return path, label

    sections = []
    placed = {"index.md"}
    for section in config.get("sections", []):
        folder = section.get("folder")
        entries = []
        for item in section.get("pages", []):
            path, label = entry_of(item, folder)
            if path in site.pages and path not in placed:
                entries.append((path, label or site.pages[path].title))
                placed.add(path)
            elif path not in site.pages:
                site.warn(f"nav.json lists {path}, which does not exist", "docs/nav.json")
        if folder:
            unlisted = [path for path in sorted(site.pages) if path.startswith(folder + "/") and path not in placed]
            entries += [(path, site.pages[path].title) for path in unlisted]
            placed.update(unlisted)
        if entries:
            sections.append((section.get("title") or title_from_name(folder or "pages"), entries))

    for folder in sorted({path.split("/")[0] for path in site.pages if "/" in path and path not in placed}):
        entries = [(path, site.pages[path].title) for path in sorted(site.pages)
                   if path.startswith(folder + "/") and path not in placed]
        sections.append((title_from_name(folder), entries))
        placed.update(path for path, _ in entries)

    loose = [(path, site.pages[path].title) for path in sorted(site.pages) if path not in placed]
    if loose:
        sections.append(("More", loose))
    site.sections = sections


# Pieces of a page --------------------------------------------------------------

GITHUB_ICON = ('<svg width="17" height="17" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M12 '
               '.297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-'
               '2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 '
               '1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-'
               '.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 '
               '1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 '
               '2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 '
               '1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-'
               '12-12"/></svg>')
MENU_BUTTON = ('<button class="icon-button menu-button" id="menu-button" type="button" aria-label="Open the menu" '
               'aria-controls="sidebar" aria-expanded="false"><svg width="18" height="18" viewBox="0 0 24 24" '
               'fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" '
               'aria-hidden="true"><path d="M4 6h16"/><path d="M4 12h16"/><path d="M4 18h16"/></svg></button>')
CHEVRON = ('<svg class="chevron" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" '
           'stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'
           '<path d="m9 18 6-6-6-6"/></svg>')
ARROW_BACK = ('<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" '
              'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m12 19-7-7 7-7"/>'
              '<path d="M19 12H5"/></svg>')
ARROW_FORWARD = ('<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" '
                 'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12h14"/>'
                 '<path d="m12 5 7 7-7 7"/></svg>')
ARROW_OUT = ('<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" '
             'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M7 7h10v10"/>'
             '<path d="M7 17 17 7"/></svg>')

# Icons for the cards on the home page, by the folder of the page a card links
# to. A folder without one gets the book.
ICONS = {
    "core": '<path d="M21 8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16Z"/><path d="m3.3 7 8.7 5 8.7-5"/><path d="M12 22V12"/>',
    "assets": '<path d="M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z"/>',
    "window": '<rect x="2" y="4" width="20" height="16" rx="2"/><path d="M10 4v4"/><path d="M2 8h20"/><path d="M6 4v4"/>',
    "input": '<line x1="6" x2="10" y1="11" y2="11"/><line x1="8" x2="8" y1="9" y2="13"/><line x1="15" x2="15.01" y1="12" y2="12"/><line x1="18" x2="18.01" y1="10" y2="10"/><path d="M17.32 5H6.68a4 4 0 0 0-3.978 3.59c-.006.052-.01.101-.017.152C2.604 9.416 2 14.456 2 16a3 3 0 0 0 3 3c1 0 1.5-.5 2-1l1.414-1.414A2 2 0 0 1 9.828 16h4.344a2 2 0 0 1 1.414.586L17 18c.5.5 1 1 2 1a3 3 0 0 0 3-3c0-1.545-.604-6.584-.685-7.258-.007-.05-.011-.1-.017-.151A4 4 0 0 0 17.32 5z"/>',
    "graphics": '<rect width="18" height="18" x="3" y="3" rx="2" ry="2"/><circle cx="9" cy="9" r="2"/><path d="m21 15-3.086-3.086a2 2 0 0 0-2.828 0L6 21"/>',
    "data": '<ellipse cx="12" cy="5" rx="9" ry="3"/><path d="M3 5V19A9 3 0 0 0 21 19V5"/><path d="M3 12A9 3 0 0 0 21 12"/>',
    "ui": '<rect width="7" height="9" x="3" y="3" rx="1"/><rect width="7" height="5" x="14" y="3" rx="1"/><rect width="7" height="9" x="14" y="12" rx="1"/><rect width="7" height="5" x="3" y="16" rx="1"/>',
    "script": '<polyline points="4 17 10 11 4 5"/><line x1="12" x2="20" y1="19" y2="19"/>',
    "sound": '<polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><path d="M15.54 8.46a5 5 0 0 1 0 7.07"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14"/>',
    "physics": '<circle cx="12" cy="12" r="1"/><path d="M20.2 20.2c2.04-2.03.02-7.36-4.5-11.9-4.54-4.52-9.87-6.54-11.9-4.5-2.04 2.03-.02 7.36 4.5 11.9 4.54 4.52 9.87 6.54 11.9 4.5Z"/><path d="M15.7 15.7c4.52-4.54 6.54-9.87 4.5-11.9-2.03-2.04-7.36-.02-11.9 4.5-4.52 4.54-6.54 9.87-4.5 11.9 2.03 2.04 7.36.02 11.9-4.5Z"/>',
    "network": '<rect x="16" y="16" width="6" height="6" rx="1"/><rect x="2" y="16" width="6" height="6" rx="1"/><rect x="9" y="2" width="6" height="6" rx="1"/><path d="M5 16v-3a1 1 0 0 1 1-1h12a1 1 0 0 1 1 1v3"/><path d="M12 12V8"/>',
    "": '<path d="M12 7v14"/><path d="M3 18a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1h5a4 4 0 0 1 4 4 4 4 0 0 1 4-4h5a1 1 0 0 1 1 1v13a1 1 0 0 1-1 1h-6a3 3 0 0 0-3 3 3 3 0 0 0-3-3z"/>',
}


def sidebar_html(site, current):
    groups = []
    for title, entries in site.sections:
        items = []
        is_open = False
        for path, label in entries:
            page = site.pages[path]
            active = page.slug == current
            is_open = is_open or active
            attributes = ' class="sidebar-link active" aria-current="page"' if active else ' class="sidebar-link"'
            items.append(f'          <li><a{attributes} href="{page_address(current, page.slug)}">'
                         f'{html.escape(label)}</a></li>')
        groups.append(f'        <details class="sidebar-group"{" open" if is_open else ""}>\n'
                      f'          <summary>{html.escape(title)}{CHEVRON}</summary>\n'
                      f'          <ul>\n' + "\n".join(items) + '\n          </ul>\n        </details>')
    return "\n".join(groups)


def outline_html(outline):
    if len(outline) < 2:
        return ""
    items = "".join(f'<li class="outline-level-{level}"><a href="#{anchor}">{html.escape(text)}</a></li>'
                    for level, text, anchor in outline)
    return f'      <p class="outline-title">On this page</p>\n      <ul class="outline-list">{items}</ul>'


def pager_html(site, order, position):
    cells = []
    for offset, kind in ((-1, "previous"), (1, "next")):
        other = position + offset
        if 0 <= other < len(order):
            page = site.pages[order[other]]
            label = f"{ARROW_BACK} Previous" if kind == "previous" else f"Next {ARROW_FORWARD}"
            relation = "prev" if kind == "previous" else "next"
            cells.append(f'<a class="pager-card {kind}" href="{page_address(site.pages[order[position]].slug, page.slug)}" '
                         f'rel="{relation}"><span class="pager-label">{label}</span>'
                         f'<span class="pager-title">{html.escape(page.title)}</span></a>')
        else:
            cells.append('<span aria-hidden="true"></span>')
    return "".join(cells)


def first_paragraph(body):
    match = re.search(r"<p>(.*?)</p>", body, re.S)
    text = html.unescape(re.sub(r"<[^>]+>", "", match.group(1))) if match else ""
    text = re.sub(r"\s+", " ", text).strip()
    return text if len(text) <= 158 else text[:155].rsplit(" ", 1)[0] + "…"


def text_of(body):
    text = re.sub(r"<pre.*?</pre>", " ", body, flags=re.S)
    text = re.sub(r"<[^>]+>", " ", text)
    return re.sub(r"\s+", " ", html.unescape(text)).strip()


def render(template_name, values, partials):
    template = read(TEMPLATES / template_name)
    template = re.sub(r"\{\{> (\w+)\}\}", lambda match: partials[match.group(1)], template)

    def swap(match):
        key = match.group(1)
        if key not in values:
            fail(f"{template_name}: nothing fills {{{{{key}}}}}")
        return values[key]

    return re.sub(r"\{\{(\w+)\}\}", swap, template)


# The home page -----------------------------------------------------------------

def split_home(text):
    """docs/index.md as its opening (without the "# " title) and its "## "
    sections, each (heading, lines)."""
    opening, sections = [], []
    fence = None
    for line in text.split("\n"):
        match = FENCE.match(line)
        if match:
            marker = match.group(2)
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
        heading = HEADING.match(line) if fence is None and not match else None
        if heading and len(heading.group(1)) == 1 and not sections:
            continue
        if heading and len(heading.group(1)) == 2:
            sections.append((heading.group(2), []))
        elif sections:
            sections[-1][1].append(line)
        else:
            opening.append(line)
    return opening, sections


def blocks_of(lines):
    """Lines split at blank lines outside code blocks."""
    blocks, current, fence = [], [], None
    for line in lines:
        match = FENCE.match(line)
        if match:
            marker = match.group(2)
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
        if not line.strip() and fence is None:
            if current:
                blocks.append(current)
            current = []
        else:
            current.append(line)
    if current:
        blocks.append(current)
    return blocks


INSIDE_TAG = "What's inside"
LINK_ONLY = re.compile(r"^\[((?:[^\[\]]|\[[^\]]*\])+)\]\(([^)\s]+)\)$")
LINK_FIRST = re.compile(r"^[-*+]\s+\[((?:[^\[\]]|\[[^\]]*\])+)\]\(([^)\s]+)\)\s*[:.,-]?\s*(.*)$")


def section_head(tag, title, anchor, sub=""):
    tag_html = f'\n          <p class="section-tag">{html.escape(tag)}</p>' if tag else ""
    return (f'      <div class="section-head">\n        <div class="section-copy reveal">{tag_html}\n'
            f'          <h2 id="{anchor}-title">{title}</h2>{sub}\n        </div>\n      </div>')


def home_section(site, renderer, heading, lines):
    slugger_anchor = slugify(plain_text(heading))
    title = renderer.inline(heading)
    blocks = blocks_of(lines)
    code = [block for block in blocks if FENCE.match(block[0])]
    rows = []
    for block in blocks:
        if len(block) > 2 and TABLE_SEPARATOR.match(block[1]):
            rows = [split_row(line) for line in block[2:]]
            break
    links = []
    if len(blocks) == 1 and all(LIST_ITEM.match(line) or line.startswith(" ") for line in blocks[0]):
        items = re.split(r"\n(?=[-*+]\s)", "\n".join(line.strip() for line in blocks[0]))
        links = [LINK_FIRST.match(item.replace("\n", " ")) for item in items]

    opening = f'    <section class="section container" id="{slugger_anchor}" aria-labelledby="{slugger_anchor}-title">'

    if code:
        first = blocks.index(code[0])
        before = "".join(f'\n          <p class="section-sub">{renderer.inline(" ".join(line.strip() for line in block))}</p>'
                         for block in blocks[:first])
        after = renderer.render("\n\n".join("\n".join(block) for block in blocks[first + 1:]))
        fence = FENCE.match(code[0][0])
        closing = len(code[0]) - 1 if FENCE.match(code[0][-1]) and len(code[0]) > 1 else len(code[0])
        language = fence.group(3)
        file_name = "main.cpp" if LANGUAGES.get(language.lower()) == "cpp" else LANGUAGE_NAMES.get(language.lower(), language)
        card = (f'      <div class="example-card reveal">\n'
                f'        <p class="example-bar"><span class="dots" aria-hidden="true"><i></i><i></i><i></i></span>'
                f'{html.escape(file_name)}</p>\n'
                f'{code_block_html(chr(10).join(code[0][1:closing]), language)}\n      </div>')
        note = f'\n      <div class="example-note reveal">{after}</div>' if after else ""
        return f"{opening}\n{section_head('Example', title, slugger_anchor, before)}\n{card}{note}\n    </section>"

    if rows and all(LINK_ONLY.match(row[0]) for row in rows):
        cards = []
        for row in rows:
            match = LINK_ONLY.match(row[0])
            target = renderer.rewrite_link(match.group(2))
            folder = posixpath.normpath(posixpath.join(posixpath.dirname(renderer.page.path), match.group(2))).split("/")[0]
            icon = ICONS.get(folder, ICONS[""])
            status = f'<span class="feature-status">{renderer.inline(row[2])}</span>' if len(row) > 2 and row[2] else ""
            text = f"<p>{renderer.inline(row[1])}</p>" if len(row) > 1 else ""
            cards.append(f'        <article class="feature reveal"><div class="feature-top"><span class="feature-icon" '
                         f'aria-hidden="true"><svg width="22" height="22" viewBox="0 0 24 24" fill="none" '
                         f'stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'
                         f'{icon}</svg></span>{status}</div><h3><a href="{html.escape(target)}">'
                         f'{renderer.inline(match.group(1))}</a></h3>{text}</article>')
        return (f"{opening}\n{section_head(INSIDE_TAG, title, slugger_anchor)}\n"
                f'      <div class="feature-grid">\n' + "\n".join(cards) + "\n      </div>\n    </section>")

    if links and all(links):
        cards = []
        for match in links:
            target = renderer.rewrite_link(match.group(2))
            words = match.group(3)
            # "[Page](page.md): what it covers" reads as a sentence on a card,
            # unless it starts with a name written in lowercase.
            if words and words[0].islower() and not words.startswith(site.project.name):
                words = words[0].upper() + words[1:]
            text = f"<p>{renderer.inline(words)}</p>" if words else ""
            cards.append(f'        <article class="feature reveal"><h3><a href="{html.escape(target)}">'
                         f'{renderer.inline(match.group(1))}</a></h3>{text}</article>')
        return (f"{opening}\n{section_head('Next steps', title, slugger_anchor)}\n"
                f'      <div class="feature-grid">\n' + "\n".join(cards) + "\n      </div>\n    </section>")

    body = renderer.render("\n".join(lines))
    return (f"{opening}\n{section_head('', title, slugger_anchor)}\n"
            f'      <div class="prose home-prose reveal">\n{body}\n      </div>\n    </section>')


def inline_text(markdown):
    """Inline Markdown as text that keeps its code spans, for short lines on
    cards, where a link would lead somewhere the card does not."""
    pieces = []
    for piece in re.split(r"(`+[^`]+`+)", markdown):
        if piece.startswith("`"):
            pieces.append(f"<code>{html.escape(piece.strip('`').strip())}</code>")
            continue
        piece = re.sub(r"\\(.)", r"\1", piece)
        piece = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", piece)
        piece = re.sub(r"<[^>]+>", "", piece)
        piece = re.sub(r"(\*\*|__|\*|_)(\S.*?\S|\S)\1", r"\2", piece)
        pieces.append(html.escape(piece))
    return re.sub(r"\s+", " ", "".join(pieces)).strip()


def latest_release(site):
    """The newest "## " section of the changelog: its heading and first paragraph."""
    page = site.pages.get("reference/changelog.md")
    if not page:
        return site.project.name + " " + site.project.version, f"<p>{html.escape(site.project.description)}</p>"
    _, sections = split_home(page.text)
    if not sections:
        return site.project.name + " " + site.project.version, ""
    heading, lines = sections[0]
    paragraphs = [block for block in blocks_of(lines) if not HEADING.match(block[0]) and not FENCE.match(block[0])
                  and not LIST_ITEM.match(block[0])]
    title = plain_text(heading)
    if re.match(r"^v?\d", title):
        title = f"{site.project.name} {title}"
    text = ""
    if paragraphs:
        text = f"          <p>{inline_text(' '.join(line.strip() for line in paragraphs[0]))}</p>"
    return title, text


def opening_paragraph(text):
    """The first paragraph of a Markdown file, before its first "## " heading."""
    opening, _ = split_home(text)
    for block in blocks_of(opening):
        if not (FENCE.match(block[0]) or QUOTE.match(block[0]) or HEADING.match(block[0])
                or LIST_ITEM.match(block[0]) or HTML_BLOCK.match(block[0])):
            return " ".join(line.strip() for line in block)
    return ""


def examples_summary(project):
    """The opening paragraph of the examples repository's README, read from
    GitHub as the site is built, so the home page describes the examples the
    way their own repository does. Empty when it cannot be read."""
    match = re.match(r"^https://github\.com/([^/]+)/([^/#?]+?)/?$", project.examples)
    if not match:
        return ""
    address = f"https://raw.githubusercontent.com/{match.group(1)}/{match.group(2)}/HEAD/README.md"
    try:
        request = urllib.request.Request(address, headers={"User-Agent": f"{project.name}-documentation"})
        with urllib.request.urlopen(request, timeout=20) as response:
            readme = response.read().decode("utf-8").replace("\r\n", "\n")
    except (OSError, ValueError) as error:
        print(f"note: {address} could not be read ({error}), so the examples card shows only its link",
              file=sys.stderr)
        return ""
    return inline_text(opening_paragraph(readme))


# Images ------------------------------------------------------------------------

def make_images(project, tagline, folder):
    """The icons, the logo shown on the home page, and the picture shared with
    links, all made from docs/logo.png. Gives the shown logo's size."""
    try:
        from PIL import Image, ImageDraw, ImageFilter, ImageFont
    except ImportError:
        fail("the images need Pillow: pip install -r .github/site/requirements.txt")
    if not LOGO.exists():
        fail("docs/logo.png is missing; the site's logo and icons are made from it")

    folder.mkdir(parents=True, exist_ok=True)
    logo = Image.open(LOGO).convert("RGBA")
    drawing = logo.crop(logo.getchannel("A").getbbox() or (0, 0, logo.width, logo.height))

    side = int(max(drawing.size) * 1.08)
    icon = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    icon.paste(drawing, ((side - drawing.width) // 2, (side - drawing.height) // 2), drawing)
    for size in (32, 96, 180, 192, 512):
        icon.resize((size, size), Image.LANCZOS).save(folder / f"icon-{size}.png", optimize=True)

    shown = drawing.resize((640, max(1, round(640 * drawing.height / drawing.width))), Image.LANCZOS)
    shown.save(folder / "logo.webp", quality=88, method=6)
    shown.save(folder / "logo.png", optimize=True)

    def font(size):
        for name in ("SpaceGrotesk-Bold.ttf", "segoeuib.ttf", "DejaVuSans-Bold.ttf", "arialbd.ttf"):
            for place in (Path("C:/Windows/Fonts"), Path("/usr/share/fonts/truetype/dejavu"),
                          Path("/usr/share/fonts/TTF"), Path("/Library/Fonts")):
                if (place / name).exists():
                    return ImageFont.truetype(str(place / name), size)
        try:
            return ImageFont.load_default(size)
        except TypeError:
            return ImageFont.load_default()

    # 1200 by 630, the size link previews use: the logo and the name on the
    # dark theme's background, with a soft glow behind the logo.
    width, height = 1200, 630
    picture = Image.new("RGBA", (width, height), (34, 38, 47, 255))
    small = drawing.resize((500, max(1, round(500 * drawing.height / drawing.width))), Image.LANCZOS)
    if small.height > 440:
        small = drawing.resize((max(1, round(440 * drawing.width / drawing.height)), 440), Image.LANCZOS)
    left, top = 80, (height - small.height) // 2
    glow = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((left - 40, top - 60, left + small.width + 40, top + small.height + 60),
                                 fill=(249, 115, 22, 70))
    picture.alpha_composite(glow.filter(ImageFilter.GaussianBlur(70)))
    picture.alpha_composite(small, (left, top))

    draw = ImageDraw.Draw(picture)
    text_left = left + small.width + 56
    room = width - text_left - 60
    name_font, line_font = font(92), font(38)
    lines, line = [], ""
    for word in tagline.split():
        trial = f"{line} {word}".strip()
        if line and draw.textlength(trial, font=line_font) > room:
            lines.append(line)
            line = word
        else:
            line = trial
    if line:
        lines.append(line)
    lines = lines[:4]
    block = 110 + 50 * len(lines)
    top = (height - block) // 2
    draw.text((text_left, top), project.name, font=name_font, fill=(233, 237, 244, 255))
    for number, text in enumerate(lines):
        draw.text((text_left + 4, top + 122 + 50 * number), text, font=line_font, fill=(154, 163, 181, 255))
    picture.convert("RGB").save(folder / "og.png", optimize=True)
    return shown.width, shown.height


# Building ----------------------------------------------------------------------

def build(output, strict, site_url):
    project = load_project(site_url)
    site = Site(project, strict)
    load_pages(site)
    load_navigation(site)
    order = [path for _, entries in site.sections for path, _ in entries]
    if not order:
        fail("docs/ has no pages")
    category_of = {}
    for title, entries in site.sections:
        for path, _ in entries:
            category_of.setdefault(path, title)
    partials = {file.stem: read(file).rstrip("\n") for file in (TEMPLATES / "partials").glob("*.html")}

    if output.exists():
        shutil.rmtree(output)
    if (DOCS / FILES).exists():
        fail(f"docs/{FILES} would collide with the folder that holds the site's own files")
    shutil.copytree(SITE_DIRECTORY / "assets", output / FILES)

    home = site.pages.get("index.md") or Page("index.md", f"# {project.name}\n", "docs/index.md")
    home_renderer = MarkdownRenderer(site, home)
    opening, home_sections = split_home(home.text)
    opening_blocks = blocks_of(opening)
    first = next((block for block in opening_blocks if not QUOTE.match(block[0]) and not FENCE.match(block[0])), None)
    tagline_markdown = " ".join(line.strip() for line in first) if first else project.description
    rest = [block for block in opening_blocks if block is not first]
    tagline_text = plain_text(tagline_markdown)

    logo_width, logo_height = make_images(project, tagline_text, output / FILES / "img")
    digest = hashlib.sha256()
    for file in sorted((output / FILES).rglob("*")):
        if file.is_file():
            digest.update(file.read_bytes())
    asset_version = digest.hexdigest()[:10]

    def common(slug, depth=None):
        if depth is None:
            depth = slug.count("/") + 1 if slug else 0
        root = "../" * depth or "./"
        return {
            "root": root,
            "assets": f"{root}{FILES}/",
            "search_url": f"{root}{FILES}/search.json",
            "shared_picture": html.escape(f"{project.site_url}{FILES}/img/og.png"),
            "docs_home": page_address(slug, site.pages[order[0]].slug),
            "changelog_page": page_address(slug, "reference/changelog") if "reference/changelog.md" in site.pages
            else html.escape(f"{project.repository}/blob/{project.branch}/CHANGELOG.md"),
            "license_page": page_address(slug, "reference/license") if "reference/license.md" in site.pages
            else html.escape(f"{project.repository}/blob/{project.branch}/LICENSE"),
            "name": html.escape(project.name),
            "version": html.escape(project.version),
            "repository": html.escape(project.repository),
            "examples": html.escape(project.examples),
            "author": html.escape(project.author),
            "author_url": html.escape(project.author_url),
            "license_name": html.escape(project.license_name),
            "copyright_year": html.escape(project.copyright_year),
            "site_url": html.escape(project.site_url),
            "asset_version": asset_version,
            "container": "container-wide" if slug else "container",
            "menu_button": "",
            "docs_active": "",
            "docs_current": "",
            "github_icon": GITHUB_ICON,
        }

    # Pages
    search = []
    for position, path in enumerate(order):
        page = site.pages[path]
        renderer = MarkdownRenderer(site, page)
        body = renderer.render(page.text)
        body = re.sub(r"^\s*<h1[^>]*>.*?</h1>\s*", "", body, count=1, flags=re.S)
        outline = renderer.outline
        values = common(page.slug)
        if page.editable:
            edit_url = f"{project.repository}/edit/{project.branch}/{page.source}"
            edit_label = "Edit this page on GitHub"
        else:
            edit_url = f"{project.repository}/blob/{project.branch}/{page.source}"
            edit_label = f"{page.source} on GitHub"
        values.update({
            "page_title": html.escape(f"{page.title} · {project.name} documentation"),
            "title": html.escape(page.title),
            "category": html.escape(category_of[path]),
            "description": html.escape(first_paragraph(body) or project.description),
            "canonical": html.escape(project.site_url + page.slug + "/"),
            "content": body,
            "sidebar": sidebar_html(site, page.slug),
            "outline": outline_html(outline),
            "pager": pager_html(site, order, position),
            "edit_url": html.escape(edit_url),
            "edit_label": html.escape(edit_label),
            "menu_button": MENU_BUTTON,
            "docs_active": " active",
            "docs_current": ' aria-current="true"',
        })
        write(output / page.slug / "index.html", render("page.html", values, partials))
        # The address the page had before pages became folders still leads to it.
        name = posixpath.basename(page.slug)
        moved = project.site_url + page.slug + "/"
        write(output / (page.slug + ".html"),
              f'<!DOCTYPE html>\n<html lang="en">\n<head>\n<meta charset="UTF-8" />\n<title>{html.escape(page.title)}</title>\n'
              f'<link rel="canonical" href="{html.escape(moved)}" />\n<meta name="robots" content="noindex" />\n'
              f'<meta http-equiv="refresh" content="0; url={name}/" />\n'
              f'<script>location.replace("{name}/" + location.hash);</script>\n</head>\n<body>\n'
              f'<p>This page is now at <a href="{name}/">{html.escape(moved)}</a>.</p>\n</body>\n</html>\n')
        search.append({"t": page.title, "c": category_of[path], "u": page.slug + "/",
                       "h": [[text, anchor] for _, text, anchor in outline], "x": text_of(body)[:8000]})
    write(output / FILES / "search.json", json.dumps(search, ensure_ascii=False, separators=(",", ":")))

    # The home page
    values = common("")
    sections = "\n\n".join(home_section(site, home_renderer, heading, lines) for heading, lines in home_sections)
    categories = []
    for title, entries in site.sections:
        links = "".join(f'<li><a href="{page_address("", site.pages[path].slug)}">{html.escape(label)}</a></li>'
                        for path, label in entries)
        categories.append(f'        <article class="category-card reveal"><h3><a href="'
                          f'{page_address("", site.pages[entries[0][0]].slug)}">{html.escape(title)}</a>'
                          f'<span class="category-count">{len(entries)}</span></h3><ul>{links}</ul></article>')
    release_title, release_text = latest_release(site)
    examples_name = project.examples.rstrip("/").rsplit("/", 1)[-1]
    example_links = f'            <li><a href="{html.escape(project.examples)}">{html.escape(examples_name)}{ARROW_OUT}</a></li>'
    examples_text = examples_summary(project)
    structured_data = json.dumps({
        "@context": "https://schema.org", "@type": "SoftwareSourceCode", "name": project.name,
        "description": project.description, "url": project.site_url, "codeRepository": project.repository,
        "programmingLanguage": "C++", "version": project.version,
        "author": {"@type": "Person", "name": project.author, "url": project.author_url},
    }, ensure_ascii=False).replace("</", "<\\/")
    lead = home_renderer.render("\n\n".join("\n".join(block) for block in rest))
    values.update({
        "page_title": html.escape(f"{project.name} · {tagline_text.rstrip('.')}"),
        "description": html.escape(project.description or tagline_text),
        "canonical": html.escape(project.site_url),
        "tagline": home_renderer.inline(tagline_markdown),
        "lead": lead,
        "logo_width": str(logo_width),
        "logo_height": str(logo_height),
        "sections": sections,
        "categories": "\n".join(categories),
        "release_title": html.escape(release_title),
        "release_text": release_text,
        "example_links": example_links,
        "examples_text": f"          <p>{examples_text}</p>" if examples_text else "",
        "structured_data": structured_data,
    })
    write(output / "index.html", render("home.html", values, partials))

    # GitHub Pages shows this for a missing address at any depth, so its links
    # are absolute.
    values = common("", depth=0)
    base = html.escape(project.site_url)
    values.update({
        "root": base, "assets": f"{base}{FILES}/", "search_url": f"{base}{FILES}/search.json",
        "docs_home": base + html.escape(site.pages[order[0]].slug) + "/",
        "changelog_page": base + "reference/changelog/", "license_page": base + "reference/license/",
        "page_title": html.escape(f"Page not found · {project.name} documentation"),
        "description": html.escape(project.description), "canonical": base,
    })
    write(output / "404.html", render("404.html", values, partials))

    addresses = [project.site_url] + [project.site_url + site.pages[path].slug + "/" for path in order]
    entries = "".join(f"<url><loc>{html.escape(address)}</loc></url>" for address in addresses)
    write(output / "sitemap.xml", '<?xml version="1.0" encoding="UTF-8"?>\n'
          f'<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">{entries}</urlset>\n')
    write(output / "robots.txt", f"User-agent: *\nAllow: /\nSitemap: {project.site_url}sitemap.xml\n")

    for relative in sorted(site.files):
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(DOCS / relative, destination)
    write(output / ".nojekyll", "")

    print(f"{project.name} {project.version}: {len(order)} pages and the home page written to {output}, "
          f"for {project.site_url}, with {len(site.warnings)} warnings")
    return 1 if strict and site.warnings else 0


def main():
    parser = argparse.ArgumentParser(description="Build the documentation site.")
    parser.add_argument("--output", default=str(REPOSITORY / "_site"), help="folder to write the site into")
    parser.add_argument("--strict", action="store_true", help="fail when there are warnings")
    parser.add_argument("--site-url", default="", help="the address the site is published at; "
                        "by default the repository's GitHub Pages address")
    arguments = parser.parse_args()
    return build(Path(arguments.output).resolve(), arguments.strict, arguments.site_url)


if __name__ == "__main__":
    sys.exit(main())
