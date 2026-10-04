"""Builds the easyforge documentation site from the Markdown files in docs/.

Every .md file under docs/ becomes a page, titled by its first "# " heading.
docs/nav.json can set the order and titles of sections and pages. Anything it
does not mention is added after what it lists, so adding a page needs no change
here. CHANGELOG.md and LICENSE from the repository root become reference pages.

Broken links and missing headings are printed as warnings. With --strict they
also make the build fail.

    python .github/site/build.py --output _site
"""

import argparse
import html
import json
import posixpath
import re
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path

SITE_DIRECTORY = Path(__file__).resolve().parent
REPOSITORY = SITE_DIRECTORY.parents[1]
REPOSITORY_URL = "https://github.com/cresmarmat-an/easyforge"
EXAMPLES_URL = "https://github.com/cresmarmat-an/easyforge-examples"


@dataclass
class Page:
    path: str  # relative to docs/, such as "core/math.md"
    text: str
    edit_url: str
    title: str = ""
    section: str = ""
    anchors: set = field(default_factory=set)

    @property
    def output(self):
        return self.path[:-3] + ".html"

    @property
    def name(self):
        return posixpath.splitext(posixpath.basename(self.path))[0]


class Site:
    def __init__(self, docs):
        self.docs = docs
        self.warnings = []
        self.pages = {}

    def warn(self, message):
        self.warnings.append(message)


# Headings and anchors ------------------------------------------------------

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
    """Gives each heading a unique anchor, adding -1, -2, ... to repeats."""

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


# Syntax highlighting -------------------------------------------------------

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


# Markdown ------------------------------------------------------------------

RULE = re.compile(r"^\s{0,3}([-*_])(\s*\1){2,}\s*$")
TABLE_SEPARATOR = re.compile(r"^\s*\|?\s*:?-+:?\s*(\|\s*:?-+:?\s*)*\|?\s*$")
LIST_ITEM = re.compile(r"^(\s*)([-*+]|\d{1,9}[.)])(\s+|$)")
QUOTE = re.compile(r"^\s{0,3}>\s?(.*)$")
CALLOUT = re.compile(r"^\[!(\w+)\][+-]?\s*(.*)$")
HTML_BLOCK = re.compile(
    r"^\s*</?(?:div|p|table|details|summary|figure|img|picture|section|aside|pre|ul|ol|h[1-6]|hr|br)\b", re.I)

CALLOUT_TITLES = {
    "note": "Note", "tip": "Tip", "important": "Important", "warning": "Warning", "caution": "Caution",
}


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
        body = highlight("\n".join(code), language)
        language_class = f' class="language-{html.escape(language)}"' if language else ""
        output.append(
            f'<div class="code-block"><pre><code{language_class}>{body}</code></pre></div>')
        return index

    def heading(self, level, markdown):
        anchor = self.slugger.anchor(plain_text(markdown))
        content = self.inline(markdown)
        if level in (2, 3):
            self.outline.append((level, content, anchor))
        return (f'<h{level} id="{anchor}">{content}'
                f'<a class="anchor" href="#{anchor}" aria-label="Link to this section">#</a></h{level}>')

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

    @staticmethod
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

    def table(self, lines, index, output):
        header = self.split_row(lines[index])
        alignments = []
        for cell in self.split_row(lines[index + 1]):
            left, right = cell.startswith(":"), cell.endswith(":")
            alignments.append("center" if left and right else "right" if right else "left" if left else None)
        index += 2

        rows = []
        while index < len(lines) and lines[index].strip() and "|" in lines[index]:
            rows.append(self.split_row(lines[index]))
            index += 1

        def cell(tag, text, column):
            alignment = alignments[column] if column < len(alignments) else None
            style = f' style="text-align: {alignment}"' if alignment else ""
            return f"<{tag}{style}>{self.inline(text)}</{tag}>"

        head = "".join(cell("th", text, column) for column, text in enumerate(header))
        if not any(text.strip() for text in header):
            head_html = ""
        else:
            head_html = f"<thead><tr>{head}</tr></thead>"
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
            external = re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) is not None
            attributes = ' rel="noopener"' if external else ""
            return hold(f'<a href="{html.escape(target)}"{attributes}>{label}</a>')

        text = re.sub(r'\[((?:[^\[\]]|\[[^\]]*\])+)\]\(([^)\s]+)(?:\s+"([^"]*)")?\)', link, text)

        text = html.escape(text, quote=False)
        text = re.sub(r"\*\*(?=\S)(.+?)(?<=\S)\*\*", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<![\w_])__(?=\S)(.+?)(?<=\S)__(?![\w_])", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<![\w*])\*(?=\S)(.+?)(?<=\S)\*(?![\w*])", r"<em>\1</em>", text)
        text = re.sub(r"(?<![\w_])_(?=\S)(.+?)(?<=\S)_(?![\w_])", r"<em>\1</em>", text)
        return text

    def rewrite_link(self, target):
        if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("//"):
            return target

        path, _, anchor = target.partition("#")
        pages = self.site.pages
        if not path:
            if anchor and anchor not in self.page.anchors:
                self.site.warn(f"{self.page.path}: link to missing heading #{anchor}")
            return target

        resolved = posixpath.normpath(posixpath.join(posixpath.dirname(self.page.path), path))
        if resolved.endswith(".md"):
            if resolved not in pages:
                self.site.warn(f"{self.page.path}: link to missing page {path}")
            elif anchor and anchor not in pages[resolved].anchors:
                self.site.warn(f"{self.page.path}: link to missing heading {path}#{anchor}")
            return path[:-3] + ".html" + (f"#{anchor}" if anchor else "")

        if not (self.site.docs / resolved).exists():
            self.site.warn(f"{self.page.path}: link to missing file {path}")
        return target


# Site ----------------------------------------------------------------------

def load_pages(site):
    for file in sorted(site.docs.rglob("*.md")):
        relative = file.relative_to(site.docs).as_posix()
        text = strip_front_matter(file.read_text(encoding="utf-8").replace("\r\n", "\n"))
        site.pages[relative] = Page(relative, text, f"{REPOSITORY_URL}/blob/main/docs/{relative}")

    changelog = REPOSITORY / "CHANGELOG.md"
    if changelog.exists():
        site.pages["reference/changelog.md"] = Page(
            "reference/changelog.md", changelog.read_text(encoding="utf-8").replace("\r\n", "\n"),
            f"{REPOSITORY_URL}/blob/main/CHANGELOG.md")

    license_file = REPOSITORY / "LICENSE"
    if license_file.exists():
        text = ("# License\n\neasyforge is released under the MIT License. A program that uses it must keep "
                "this notice with its copies of easyforge.\n\n```text\n"
                + license_file.read_text(encoding="utf-8").replace("\r\n", "\n").rstrip() + "\n```\n")
        site.pages["reference/license.md"] = Page("reference/license.md", text, f"{REPOSITORY_URL}/blob/main/LICENSE")

    for page in site.pages.values():
        headings = headings_of(page.text)
        titles = [markdown for level, markdown in headings if level == 1]
        page.title = plain_text(titles[0]) if titles else title_from_name(page.name)
        page.section = page.path.split("/")[0] if "/" in page.path else ""
        slugger = Slugger()
        page.anchors = {slugger.anchor(plain_text(markdown)) for _, markdown in headings}


def load_navigation(site):
    """Sections in order, as (title, [(page path, label in the navigation)]).

    A section in nav.json either names a folder, whose pages it lists by name
    and whose unlisted pages follow, or lists pages from anywhere by their path
    without ".md". A page can be given as {"page": ..., "title": ...} to show a
    shorter label in the navigation than its own title.
    """
    navigation_file = site.docs / "nav.json"
    config = json.loads(navigation_file.read_text(encoding="utf-8")) if navigation_file.exists() else {}

    def entry_of(item, folder):
        name, label = (item["page"], item.get("title")) if isinstance(item, dict) else (item, None)
        path = f"{folder}/{name}.md" if folder else f"{name}.md"
        return path, label

    sections = []
    placed = set()
    for section in config.get("sections", []):
        folder = section.get("folder")
        entries = []
        for item in section.get("pages", []):
            path, label = entry_of(item, folder)
            if path in site.pages:
                entries.append((path, label or site.pages[path].title))
            else:
                site.warn(f"nav.json lists {path}, which does not exist")
        if folder:
            listed = {path for path, _ in entries}
            entries += [(path, site.pages[path].title) for path in sorted(site.pages)
                        if path.startswith(folder + "/") and path not in listed]
        title = section.get("title") or title_from_name(folder or "pages")
        sections.append((title, entries))
        placed.update(path for path, _ in entries)

    remaining_folders = sorted({page.section for page in site.pages.values()
                                if page.section and page.path not in placed})
    for folder in remaining_folders:
        entries = [(path, site.pages[path].title) for path in sorted(site.pages)
                   if path.startswith(folder + "/") and path not in placed]
        sections.append((title_from_name(folder), entries))
        placed.update(path for path, _ in entries)

    top_level = [(path, site.pages[path].title) for path in sorted(site.pages)
                 if "/" not in path and path != "index.md" and path not in placed]
    if top_level:
        sections.insert(0, ("Pages", top_level))
    return sections


def relative_root(page_path):
    depth = page_path.count("/")
    return "../" * depth


def relative_link(from_path, to_output):
    return relative_root(from_path) + to_output


def render_navigation(site, sections, current):
    parts = []
    if "index.md" in site.pages:
        active = ' aria-current="page"' if current == "index.md" else ""
        parts.append(f'<a class="home-link" href="{relative_link(current, "index.html")}"{active}>Home</a>')
    for title, entries in sections:
        links = []
        for path, label in entries:
            page = site.pages[path]
            active = ' aria-current="page"' if path == current else ""
            links.append(f'<li><a href="{relative_link(current, page.output)}"{active}>{html.escape(label)}</a></li>')
        parts.append(f'<section><h2>{html.escape(title)}</h2><ul>{"".join(links)}</ul></section>')
    return "\n".join(parts)


def render_outline(outline):
    if len(outline) < 2:
        return ""
    items = "".join(
        f'<li class="level-{level}"><a href="#{anchor}">{re.sub(r"<[^>]+>", "", content)}</a></li>'
        for level, content, anchor in outline)
    return f'<nav aria-label="On this page"><h2>On this page</h2><ul>{items}</ul></nav>'


def render_pager(site, order, current):
    if current not in order:
        return ""
    position = order.index(current)
    parts = []
    if position > 0:
        previous = site.pages[order[position - 1]]
        parts.append(f'<a class="previous" href="{relative_link(current, previous.output)}">'
                     f'<span>Previous</span>{html.escape(previous.title)}</a>')
    if position + 1 < len(order):
        following = site.pages[order[position + 1]]
        parts.append(f'<a class="next" href="{relative_link(current, following.output)}">'
                     f'<span>Next</span>{html.escape(following.title)}</a>')
    return f'<nav class="pager" aria-label="Pages">{"".join(parts)}</nav>' if parts else ""


def description_of(body_html):
    match = re.search(r"<p>(.*?)</p>", body_html, re.S)
    if not match:
        return "easyforge documentation"
    text = html.unescape(re.sub(r"<[^>]+>", "", match.group(1)))
    text = re.sub(r"\s+", " ", text).strip()
    return text if len(text) <= 160 else text[:157].rsplit(" ", 1)[0] + "..."


def version_text():
    """The version in CMakeLists.txt, with its label when it has one: "0.0.1"."""
    text = (REPOSITORY / "CMakeLists.txt").read_text(encoding="utf-8")
    number = re.search(r"project\(easyforge\s+VERSION\s+([\d.]+)", text)
    label = re.search(r'set\(EASYFORGE_VERSION_LABEL\s+"([^"]*)"\)', text)
    if not number:
        return ""
    return number.group(1) + (f"-{label.group(1)}" if label and label.group(1) else "")


def render_hero(body, root):
    """The home page's opening: its title and first paragraph, beside the logo.
    Gives the hero and the body without them."""
    match = re.match(r"\s*<h1\b[^>]*>(.*?)</h1>\s*<p>(.*?)</p>", body, re.S)
    if not match:
        return "", body
    title = re.sub(r'<a class="anchor".*?</a>', "", match.group(1), flags=re.S)
    version = version_text()
    facts = [f"Version {version}"] if version else []
    facts += ["C++20", "No third-party code", "MIT"]
    separator = '<span class="eyebrow-separator" aria-hidden="true"></span>'
    eyebrow = separator.join(f"<span>{html.escape(fact)}</span>" for fact in facts)
    hero = f"""<section class="hero" aria-labelledby="hero-title">
    <div class="hero-text">
        <p class="eyebrow">{eyebrow}</p>
        <h1 id="hero-title"><span class="accent">{title}</span></h1>
        <p class="hero-lead">{match.group(2)}</p>
        <div class="cta-row">
            <a class="btn btn-primary" href="{root}getting-started/introduction.html">Get started
                <svg width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12h14"/><path d="m12 5 7 7-7 7"/></svg></a>
            <a class="btn btn-soft" href="{EXAMPLES_URL}">See the examples</a>
        </div>
    </div>
    <div class="hero-logo">
        <div class="logo-float">
            <picture>
                <source srcset="{root}logo.webp" type="image/webp">
                <img src="{root}logo.png" alt="The easyforge logo: an anvil with braces on its face" width="600" height="310">
            </picture>
        </div>
    </div>
</section>"""
    return hero, body[match.end():]


def fill(template, values):
    return re.sub(r"\{\{(\w+)\}\}", lambda match: values.get(match.group(1), ""), template)


def build(output, strict):
    site = Site(REPOSITORY / "docs")
    load_pages(site)
    sections = load_navigation(site)
    order = (["index.md"] if "index.md" in site.pages else []) + [path for _, entries in sections for path, _ in entries]
    template = (SITE_DIRECTORY / "template.html").read_text(encoding="utf-8")

    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)

    for path, page in site.pages.items():
        renderer = MarkdownRenderer(site, page)
        body = renderer.render(page.text)
        root = relative_root(path)
        hero = ""
        if path == "index.md":
            hero, body = render_hero(body, root)
        title = "easyforge" if path == "index.md" else f"{page.title} · easyforge"
        document = fill(template, {
            "title": html.escape(title),
            "description": html.escape(description_of(body)),
            "root": root,
            "navigation": render_navigation(site, sections, path),
            "body": body,
            "outline": render_outline(renderer.outline),
            "pager": render_pager(site, order, path),
            "edit": html.escape(page.edit_url),
            "repository": REPOSITORY_URL,
            "examples": EXAMPLES_URL,
            "hero": hero,
            "kind": "home" if path == "index.md" else "page",
        })
        destination = output / page.output
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(document, encoding="utf-8")

    missing = Page("404.md", "", REPOSITORY_URL, title="Page not found")
    missing_body = ('<h1 id="page-not-found">Page not found</h1><p>There is no page at this address. '
                    'It may have moved; the navigation lists every page.</p>')
    (output / "404.html").write_text(fill(template, {
        "title": "Page not found · easyforge",
        "description": "This page does not exist.",
        "root": "/easyforge/",
        "navigation": render_navigation(site, sections, missing.path).replace('href="', 'href="/easyforge/'),
        "body": missing_body,
        "repository": REPOSITORY_URL,
        "examples": EXAMPLES_URL,
        "edit": REPOSITORY_URL,
        "kind": "page",
    }), encoding="utf-8")

    for asset in ("style.css", "script.js", "particles.js", "icon-32.png", "icon-180.png", "icon-192.png", "logo.png", "logo.webp"):
        shutil.copy2(SITE_DIRECTORY / asset, output / asset)
    for file in site.docs.rglob("*"):
        if file.is_file() and file.suffix != ".md" and file.name != "nav.json":
            destination = output / file.relative_to(site.docs)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, destination)
    (output / ".nojekyll").write_text("", encoding="utf-8")

    for warning in site.warnings:
        print(f"warning: {warning}", file=sys.stderr)
    print(f"Built {len(site.pages)} pages into {output} with {len(site.warnings)} warnings")
    return 1 if strict and site.warnings else 0


def main():
    parser = argparse.ArgumentParser(description="Build the easyforge documentation site.")
    parser.add_argument("--output", default=str(REPOSITORY / "_site"), help="folder to write the site into")
    parser.add_argument("--strict", action="store_true", help="fail when there are warnings")
    arguments = parser.parse_args()
    return build(Path(arguments.output).resolve(), arguments.strict)


if __name__ == "__main__":
    sys.exit(main())
