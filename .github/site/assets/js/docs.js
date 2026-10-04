/* What the documentation pages do: the theme switch shared with
   cresmarmat-an.github.io (the same storage key, so a choice made on one site
   holds on the other), the page list that slides in on narrow screens, the
   "On this page" outline that follows the reader, copy buttons on code, the
   reading progress bar, and search over the index the build writes. */
(() => {
  const root = document.documentElement;
  const calmMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;
  const siteRoot = root.dataset.root || "./";

  /* Theme: system, then light, then dark */

  const themeButton = document.getElementById("theme-toggle");
  const darkQuery = matchMedia("(prefers-color-scheme: dark)");
  const themeNames = { auto: "System", light: "Light", dark: "Dark" };
  const themeOrder = ["auto", "light", "dark"];
  const themeColors = [...document.querySelectorAll('meta[name="theme-color"]')]
    .map((meta) => ({ meta, content: meta.content, media: meta.media }));

  function themeMode() {
    return themeOrder.includes(root.dataset.theme) ? root.dataset.theme : "auto";
  }

  function showTheme() {
    const mode = themeMode();
    const shown = mode === "auto" ? (darkQuery.matches ? "dark" : "light") : mode;
    const next = themeOrder[(themeOrder.indexOf(mode) + 1) % themeOrder.length];
    const name = themeNames[mode] + (mode === "auto" ? " (" + shown + ")" : "");
    if (themeButton) {
      themeButton.setAttribute("aria-label", "Color theme: " + name + ". Switch to " + themeNames[next].toLowerCase());
      themeButton.title = "Theme: " + name;
    }
    // The browser's own colors follow a chosen theme.
    themeColors.forEach(({ meta, content, media }) => {
      if (mode === "auto") {
        meta.content = content;
        if (media) meta.media = media;
      } else {
        meta.content = shown === "dark" ? "#22262F" : "#E0E5EC";
        meta.removeAttribute("media");
      }
    });
  }

  if (themeButton) {
    themeButton.addEventListener("click", () => {
      const next = themeOrder[(themeOrder.indexOf(themeMode()) + 1) % themeOrder.length];
      root.dataset.theme = next;
      try {
        localStorage.setItem("cm-theme", next);
      } catch (error) {
        // Without storage the choice lasts for this page only.
      }
      showTheme();
    });
  }
  darkQuery.addEventListener("change", showTheme);

  /* Sections rise into view as they are reached */

  const waiting = document.querySelectorAll(".reveal:not(.in)");
  if (!("IntersectionObserver" in window)) {
    waiting.forEach((element) => element.classList.add("in"));
  } else {
    const revealer = new IntersectionObserver((entries) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;
        entry.target.classList.add("in");
        revealer.unobserve(entry.target);
      });
    }, { threshold: 0.08, rootMargin: "0px 0px -4% 0px" });
    waiting.forEach((element) => revealer.observe(element));
  }

  /* The page list, a drawer on narrow screens */

  const sidebar = document.getElementById("sidebar");
  const scrim = document.getElementById("scrim");
  const menuButton = document.getElementById("menu-button");
  const closeButton = document.getElementById("sidebar-close");

  function setDrawer(open) {
    if (!sidebar) return;
    sidebar.classList.toggle("open", open);
    if (scrim) scrim.classList.toggle("open", open);
    if (menuButton) menuButton.setAttribute("aria-expanded", String(open));
    document.body.style.overflow = open ? "hidden" : "";
    if (open) {
      const current = sidebar.querySelector(".sidebar-link.active") || sidebar.querySelector("a");
      if (current) current.focus({ preventScroll: true });
    } else if (menuButton && sidebar.contains(document.activeElement)) {
      menuButton.focus();
    }
  }

  if (menuButton) menuButton.addEventListener("click", () => setDrawer(!sidebar.classList.contains("open")));
  if (closeButton) closeButton.addEventListener("click", () => setDrawer(false));
  if (scrim) scrim.addEventListener("click", () => setDrawer(false));
  matchMedia("(min-width: 961px)").addEventListener("change", (event) => {
    if (event.matches) setDrawer(false);
  });

  // Scrolls a long list so the current page is in view. Only the list moves;
  // scrollIntoView would also move the page.
  const currentLink = sidebar && sidebar.querySelector(".sidebar-link.active");
  if (currentLink) {
    const box = currentLink.getBoundingClientRect();
    const frame = sidebar.getBoundingClientRect();
    const top = sidebar.scrollTop + box.top - frame.top - sidebar.clientHeight / 2 + box.height / 2;
    if (top > 0) sidebar.scrollTop = top;
  }

  /* Links to headings */

  document.querySelectorAll(".prose h2[id], .prose h3[id], .prose h4[id]").forEach((heading) => {
    const anchor = document.createElement("a");
    anchor.className = "heading-anchor";
    anchor.href = "#" + heading.id;
    anchor.setAttribute("aria-label", "Link to " + heading.textContent.trim());
    anchor.textContent = "#";
    heading.prepend(anchor);
  });

  /* Copy buttons on code */

  const copyIcon = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect width="14" height="14" x="8" y="8" rx="2" ry="2"/><path d="M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2"/></svg>';
  const copiedIcon = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M20 6 9 17l-5-5"/></svg>';
  document.querySelectorAll(".highlight").forEach((block) => {
    const code = block.querySelector("pre");
    if (!code || !navigator.clipboard) return;
    const button = document.createElement("button");
    button.type = "button";
    button.className = "copy-button";
    button.setAttribute("aria-label", "Copy code");
    button.innerHTML = copyIcon;
    button.addEventListener("click", async () => {
      try {
        await navigator.clipboard.writeText(code.innerText.replace(/\n$/, ""));
        button.classList.add("done");
        button.innerHTML = copiedIcon;
        button.setAttribute("aria-label", "Copied");
        setTimeout(() => {
          button.classList.remove("done");
          button.innerHTML = copyIcon;
          button.setAttribute("aria-label", "Copy code");
        }, 1600);
      } catch (error) {
        // The browser refused the clipboard; nothing changed.
      }
    });
    block.appendChild(button);
  });

  /* The outline follows the section being read; the bar shows how far */

  const outlineLinks = [...document.querySelectorAll(".outline-list a")];
  const headings = outlineLinks
    .map((link) => document.getElementById(decodeURIComponent(link.hash.slice(1))))
    .filter(Boolean);

  function followReading() {
    if (!headings.length) return;
    const line = innerHeight * 0.3;
    let current = headings[0];
    for (const heading of headings) {
      if (heading.getBoundingClientRect().top <= line) current = heading;
    }
    if (innerHeight + scrollY >= document.documentElement.scrollHeight - 4) current = headings[headings.length - 1];
    outlineLinks.forEach((link) => link.classList.toggle("active", link.hash === "#" + current.id));
  }

  const progress = document.getElementById("progress");
  let waitingForFrame = false;

  function onScroll() {
    if (waitingForFrame) return;
    waitingForFrame = true;
    requestAnimationFrame(() => {
      waitingForFrame = false;
      followReading();
      if (progress && !calmMotion) {
        const distance = document.documentElement.scrollHeight - innerHeight;
        progress.style.transform = "scaleX(" + (distance > 0 ? Math.min(scrollY / distance, 1).toFixed(4) : 0) + ")";
      }
    });
  }

  addEventListener("scroll", onScroll, { passive: true });
  addEventListener("resize", onScroll, { passive: true });

  /* Search */

  const dialog = document.getElementById("search-dialog");
  const input = document.getElementById("search-input");
  const list = document.getElementById("search-results");
  let index = null;
  let loading = null;
  let results = [];
  let selected = 0;

  // The index has one entry a page: t is its title, c its category, u its
  // address, h its headings as [text, anchor], and x its text.
  function loadIndex() {
    if (index) return Promise.resolve(index);
    if (!loading) {
      loading = fetch(root.dataset.search)
        .then((response) => {
          if (!response.ok) throw new Error(response.status);
          return response.json();
        })
        .then((pages) => {
          index = pages.map((page) => ({
            ...page,
            lowerTitle: page.t.toLowerCase(),
            lowerHeadings: page.h.map(([text, anchor]) => [text, anchor, text.toLowerCase()]),
            lowerText: page.x.toLowerCase(),
          }));
          return index;
        })
        .catch(() => {
          loading = null;
          return null;
        });
    }
    return loading;
  }

  const escapeHtml = (text) => text.replace(/[&<>"']/g, (character) =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[character]));

  function mark(text, terms) {
    let marked = escapeHtml(text);
    for (const term of terms) {
      if (term.length < 2) continue;
      const pattern = term.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
      marked = marked.replace(new RegExp("(" + pattern + ")", "ig"), "<mark>$1</mark>");
    }
    return marked;
  }

  function excerpt(page, terms) {
    const first = terms.reduce((best, term) => {
      const at = page.lowerText.indexOf(term);
      return at >= 0 && (best < 0 || at < best) ? at : best;
    }, -1);
    if (first < 0) return page.x.slice(0, 150);
    const start = Math.max(0, page.x.lastIndexOf(" ", Math.max(0, first - 50)) + 1);
    return (start > 0 ? "…" : "") + page.x.slice(start, start + 170) + "…";
  }

  function search(query) {
    const lowerQuery = query.trim().toLowerCase();
    if (!lowerQuery || !index) return [];
    const terms = lowerQuery.split(/\s+/).filter(Boolean);
    const found = [];
    for (const page of index) {
      let score = 0;
      let heading = null;
      let everyTerm = true;
      for (const term of terms) {
        let termScore = 0;
        if (page.lowerTitle.includes(term)) termScore += page.lowerTitle.startsWith(term) ? 60 : 40;
        for (const entry of page.lowerHeadings) {
          if (entry[2].includes(term)) {
            termScore += 18;
            if (!heading) heading = entry;
          }
        }
        const count = page.lowerText.split(term).length - 1;
        if (count) termScore += Math.min(count, 12) * 2;
        if (!termScore) {
          everyTerm = false;
          break;
        }
        score += termScore;
      }
      if (!everyTerm) continue;
      if (page.lowerTitle === lowerQuery) score += 100;
      found.push({ page, score, heading, terms });
    }
    return found.sort((first, second) => second.score - first.score).slice(0, 12);
  }

  function show(query) {
    if (!list) return;
    results = search(query);
    selected = 0;
    if (!query.trim()) {
      list.innerHTML = '<li class="search-empty">Type to search the documentation.</li>';
      return;
    }
    if (!index) {
      list.innerHTML = '<li class="search-empty">The search index could not be loaded.</li>';
      return;
    }
    if (!results.length) {
      list.innerHTML = '<li class="search-empty">Nothing matches <strong>' + escapeHtml(query) + "</strong>.</li>";
      return;
    }
    list.innerHTML = results.map((result, position) => {
      const address = siteRoot + result.page.u + (result.heading ? "#" + result.heading[1] : "");
      const title = result.heading && !result.page.lowerTitle.includes(result.terms[0])
        ? result.page.t + " › " + result.heading[0]
        : result.page.t;
      return '<li><a class="search-result' + (position === 0 ? " selected" : "") + '" href="' + address +
        '" role="option" aria-selected="' + (position === 0) + '">' +
        '<span class="result-top"><span class="result-title">' + mark(title, result.terms) + "</span>" +
        '<span class="result-category">' + escapeHtml(result.page.c) + "</span></span>" +
        '<span class="result-text">' + mark(excerpt(result.page, result.terms), result.terms) + "</span></a></li>";
    }).join("");
  }

  function choose(position) {
    const items = list.querySelectorAll(".search-result");
    if (!items.length) return;
    selected = (position + items.length) % items.length;
    items.forEach((item, number) => {
      item.classList.toggle("selected", number === selected);
      item.setAttribute("aria-selected", String(number === selected));
    });
    items[selected].scrollIntoView({ block: "nearest" });
  }

  function openSearch() {
    if (!dialog) return;
    if (!dialog.open) dialog.showModal();
    input.select();
    loadIndex().then(() => show(input.value));
  }

  document.querySelectorAll("[data-search-open]").forEach((opener) => opener.addEventListener("click", openSearch));
  if (dialog) {
    input.addEventListener("input", () => loadIndex().then(() => show(input.value)));
    input.addEventListener("keydown", (event) => {
      if (event.key === "ArrowDown") {
        event.preventDefault();
        choose(selected + 1);
      } else if (event.key === "ArrowUp") {
        event.preventDefault();
        choose(selected - 1);
      } else if (event.key === "Enter") {
        const item = list.querySelectorAll(".search-result")[selected];
        if (item) {
          event.preventDefault();
          location.href = item.href;
          dialog.close();
        }
      }
    });
    dialog.addEventListener("click", (event) => {
      if (event.target === dialog) dialog.close();
    });
    list.addEventListener("click", (event) => {
      if (event.target.closest(".search-result")) dialog.close();
    });
  }

  addEventListener("keydown", (event) => {
    const typing = event.target.closest && event.target.closest("input, textarea, select, [contenteditable]");
    if ((event.key === "k" || event.key === "K") && (event.ctrlKey || event.metaKey)) {
      event.preventDefault();
      openSearch();
    } else if (event.key === "/" && !typing && !event.ctrlKey && !event.metaKey && !event.altKey) {
      event.preventDefault();
      openSearch();
    } else if (event.key === "Escape" && sidebar && sidebar.classList.contains("open")) {
      setDrawer(false);
    }
  });

  const apple = /Mac|iPhone|iPad/.test(navigator.platform || navigator.userAgent);
  document.querySelectorAll("[data-shortcut-key]").forEach((key) => {
    key.textContent = apple ? "⌘K" : "Ctrl K";
  });

  showTheme();
  followReading();
  onScroll();
})();
