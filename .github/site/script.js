(function () {
    var root = document.documentElement;

    function saveTheme(value) {
        try {
            localStorage.setItem("easyforge-theme", value);
        } catch (error) {
            // Storage can be unavailable in private windows; the choice then lasts for this page only.
        }
    }

    function currentTheme() {
        var chosen = root.getAttribute("data-theme");
        if (chosen) return chosen;
        return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
    }

    var themeButton = document.querySelector(".theme-button");
    if (themeButton) {
        themeButton.addEventListener("click", function () {
            var next = currentTheme() === "dark" ? "light" : "dark";
            root.setAttribute("data-theme", next);
            saveTheme(next);
        });
    }

    var menuButton = document.querySelector(".menu-button");
    if (menuButton) {
        menuButton.addEventListener("click", function () {
            var open = document.body.classList.toggle("navigation-open");
            menuButton.setAttribute("aria-expanded", open ? "true" : "false");
        });
        document.addEventListener("keydown", function (event) {
            if (event.key === "Escape" && document.body.classList.contains("navigation-open")) {
                document.body.classList.remove("navigation-open");
                menuButton.setAttribute("aria-expanded", "false");
            }
        });
    }

    // Scrolls the list so the current page is in view. Only the list's own
    // vertical position changes; scrollIntoView would also move a hidden list sideways.
    var sidebar = document.querySelector(".sidebar");
    var current = document.querySelector('.sidebar a[aria-current="page"]');
    if (sidebar && current) {
        sidebar.scrollTop = Math.max(0, current.offsetTop - sidebar.clientHeight / 2);
    }

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
})();
