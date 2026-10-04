(function () {
    var root = document.documentElement;
    var calmMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;

    /* Theme: system, then light, then dark, as on the author's site. */

    var themeButton = document.getElementById("theme-toggle");
    var darkQuery = matchMedia("(prefers-color-scheme: dark)");
    var themeNames = { auto: "System", light: "Light", dark: "Dark" };
    var themeOrder = ["auto", "light", "dark"];
    var themeColors = Array.prototype.map.call(document.querySelectorAll('meta[name="theme-color"]'), function (meta) {
        return { meta: meta, content: meta.content, media: meta.media };
    });

    function themeMode() {
        return themeOrder.indexOf(root.dataset.theme) >= 0 ? root.dataset.theme : "auto";
    }

    function showTheme() {
        var mode = themeMode();
        var shown = mode === "auto" ? (darkQuery.matches ? "dark" : "light") : mode;
        var next = themeOrder[(themeOrder.indexOf(mode) + 1) % themeOrder.length];
        var name = themeNames[mode] + (mode === "auto" ? " (" + shown + ")" : "");
        themeButton.setAttribute("aria-label", "Color theme: " + name + ". Switch to " + themeNames[next].toLowerCase());
        themeButton.title = "Theme: " + name;
        // The browser's own colors follow a chosen theme.
        themeColors.forEach(function (color) {
            if (mode === "auto") {
                color.meta.content = color.content;
                color.meta.media = color.media;
            } else {
                color.meta.content = shown === "dark" ? "#22262F" : "#E0E5EC";
                color.meta.removeAttribute("media");
            }
        });
    }

    if (themeButton) {
        themeButton.addEventListener("click", function () {
            var next = themeOrder[(themeOrder.indexOf(themeMode()) + 1) % themeOrder.length];
            root.dataset.theme = next;
            try {
                localStorage.setItem("cm-theme", next);
            } catch (error) {
                // Without storage the choice lasts for this page only.
            }
            showTheme();
        });
        darkQuery.addEventListener("change", showTheme);
        showTheme();
    }

    /* The list of pages, which slides in on narrow screens. */

    var menuButton = document.querySelector(".menu-button");
    var sidebar = document.querySelector(".sidebar");

    function closeMenu() {
        document.body.classList.remove("navigation-open");
        if (menuButton) menuButton.setAttribute("aria-expanded", "false");
    }

    if (menuButton) {
        menuButton.addEventListener("click", function (event) {
            var open = document.body.classList.toggle("navigation-open");
            menuButton.setAttribute("aria-expanded", open ? "true" : "false");
            event.stopPropagation();
        });
        document.addEventListener("keydown", function (event) {
            if (event.key === "Escape") closeMenu();
        });
        document.addEventListener("click", function (event) {
            if (document.body.classList.contains("navigation-open") && sidebar && !sidebar.contains(event.target)) closeMenu();
        });
    }

    // Scrolls the list so the current page is in view. Only the list's own
    // position changes; scrollIntoView would also move a hidden list sideways.
    var current = document.querySelector('.sidebar a[aria-current="page"]');
    if (sidebar && current) {
        sidebar.scrollTop = Math.max(0, current.offsetTop - sidebar.clientHeight / 2);
    }

    /* Copy buttons on code */

    document.querySelectorAll(".code-block").forEach(function (block) {
        var code = block.querySelector("code");
        if (!code || !navigator.clipboard) return;

        var button = document.createElement("button");
        button.type = "button";
        button.className = "copy-button";
        button.textContent = "Copy";
        button.addEventListener("click", function () {
            navigator.clipboard.writeText(code.innerText).then(function () {
                button.textContent = "Copied";
                setTimeout(function () { button.textContent = "Copy"; }, 1500);
            }, function () {});
        });
        block.appendChild(button);
    });

    /* Back to the top */

    var toTop = document.querySelector(".to-top");
    if (toTop) {
        toTop.addEventListener("click", function (event) {
            event.preventDefault();
            window.scrollTo({ top: 0, behavior: calmMotion ? "auto" : "smooth" });
        });
    }

    /* Reading progress, and the outline's current section, once a frame */

    var progress = document.getElementById("progress");
    var outlineLinks = Array.prototype.slice.call(document.querySelectorAll(".outline a[href^='#']"));
    var headings = outlineLinks.map(function (link) {
        return document.getElementById(decodeURIComponent(link.getAttribute("href").slice(1)));
    });
    var waiting = false;

    function update() {
        waiting = false;
        var most = document.documentElement.scrollHeight - innerHeight;
        if (progress && !calmMotion) {
            progress.style.transform = "scaleX(" + (most > 0 ? Math.min(scrollY / most, 1).toFixed(4) : 0) + ")";
        }
        if (!outlineLinks.length) return;
        // The current section is the last one whose heading has passed a line a
        // quarter of the way down the window.
        var line = innerHeight * 0.25;
        var chosen = 0;
        headings.forEach(function (heading, index) {
            if (heading && heading.getBoundingClientRect().top <= line) chosen = index;
        });
        if (scrollY + innerHeight >= document.documentElement.scrollHeight - 4) chosen = outlineLinks.length - 1;
        outlineLinks.forEach(function (link, index) {
            link.classList.toggle("current", index === chosen);
        });
    }

    function schedule() {
        if (waiting) return;
        waiting = true;
        requestAnimationFrame(update);
    }

    addEventListener("scroll", schedule, { passive: true });
    addEventListener("resize", schedule, { passive: true });
    update();
})();
