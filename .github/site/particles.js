/* The node field behind the page, as on the author's site: drifting dots, links
   between near ones, a push from the pointer, a pull while it is held down, and a
   ring where it clicks.

   - every node moves first and is drawn second, so lines end exactly on their dots
   - neighbors come from a grid of cells, each pair checked once
   - movement is timed to 60 frames a second, so fast screens behave the same
   - pointer speed is measured against event times, so every mouse pushes alike
   - the canvas matches its box at the screen's pixel ratio, up to 2, and resizing
     keeps every node where it was
   - colors follow the page's theme as well as the system's setting */
(function () {
    var canvas = document.getElementById("background");
    if (!canvas || !canvas.getContext) return;
    var context = canvas.getContext("2d");
    var root = document.documentElement;
    var reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");
    var darkQuery = matchMedia("(prefers-color-scheme: dark)");

    var palettes = {
        light: { dots: ["#2563EB", "#7C3AED", "#0891B2", "#3B82F6", "#8B5CF6", "#0EA5E9"], link: "59,91,160", linkAlpha: 0.36, grab: "37,99,235", grabAlpha: 0.5, ring: "37,99,235", ringSecond: "124,58,237" },
        dark: { dots: ["#60A5FA", "#A78BFA", "#22D3EE", "#93C5FD", "#C4B5FD", "#38BDF8"], link: "148,180,255", linkAlpha: 0.3, grab: "147,197,253", grabAlpha: 0.45, ring: "147,197,253", ringSecond: "167,139,250" }
    };

    function isDark() {
        var theme = root.dataset.theme;
        return theme === "dark" || (theme !== "light" && darkQuery.matches);
    }

    var palette = palettes[isDark() ? "dark" : "light"];

    var Margin = 24;        // nodes wrap this far outside the edges, so they never pop in
    var CruiseSpeed = 0.36; // drift, in pixels per 60th of a second
    var MaximumSpeed = 4;
    var Levels = 12;        // line opacities are drawn as this many paths a frame

    var width = 0, height = 0, pixelRatio = 1, smallScreen = false;
    var linkDistance = 130, grabDistance = 190;
    var nodes = [], sparks = [], waves = [];
    var simulationTime = 0;
    var pointer = { x: 0, y: 0, velocityX: 0, velocityY: 0, time: 0, active: false, down: false, type: "mouse" };

    /* Nodes */

    function makeNode(x, y, fadeIn) {
        var angle = Math.random() * Math.PI * 2;
        var speed = CruiseSpeed * (0.35 + Math.random() * 0.9);
        var baseX = Math.cos(angle) * speed, baseY = Math.sin(angle) * speed;
        return {
            x: x, y: y, velocityX: baseX, velocityY: baseY, baseVelocityX: baseX, baseVelocityY: baseY,
            radius: 0.9 + Math.random() * 1.6,
            color: (Math.random() * 6) | 0,
            twinkle: Math.random() * Math.PI * 2,
            twinkleSpeed: 0.02 + Math.random() * 0.035,
            alpha: fadeIn ? 0 : 1
        };
    }

    function targetCount() {
        var areaPerNode = smallScreen ? 8500 : 7000;
        return Math.max(24, Math.min(smallScreen ? 80 : 190, Math.round((width * height) / areaPerNode)));
    }

    // Adds or removes nodes to suit the window's area; the others stay put.
    function fitCount(fadeIn) {
        var wanted = targetCount();
        while (nodes.length < wanted) nodes.push(makeNode(Math.random() * width, Math.random() * height, fadeIn));
        if (nodes.length > wanted) nodes.length = wanted;
    }

    function resize() {
        var newWidth = canvas.clientWidth, newHeight = canvas.clientHeight;
        if (!newWidth || !newHeight) return;
        var newRatio = Math.min(window.devicePixelRatio || 1, 2);
        if (newWidth === width && newHeight === height && newRatio === pixelRatio) return;

        // A new width scales the positions, so the field keeps its shape; a new
        // height alone, such as a phone's toolbar sliding, leaves them be.
        if (width && height && newWidth !== width) {
            var scaleX = newWidth / width, scaleY = newHeight / height;
            nodes.forEach(function (node) { node.x *= scaleX; node.y *= scaleY; });
        }
        width = newWidth;
        height = newHeight;
        pixelRatio = newRatio;
        canvas.width = Math.round(width * pixelRatio);
        canvas.height = Math.round(height * pixelRatio);
        context.setTransform(pixelRatio, 0, 0, pixelRatio, 0, 0);

        smallScreen = Math.min(width, height) < 600;
        linkDistance = smallScreen ? 108 : 132;
        grabDistance = smallScreen ? 150 : 200;
        fitCount(!reducedMotion.matches);
        if (!running()) draw(false);
    }

    /* The grid of cells that finds neighbors */

    var cellHeads = new Int32Array(0), nextInCell = new Int32Array(0), columns = 0, rows = 0;
    var linkSegments = [], grabSegments = [];
    for (var level = 0; level < Levels; level++) {
        linkSegments.push([]);
        grabSegments.push([]);
    }

    function buildGrid() {
        var cell = linkDistance;
        columns = Math.max(1, Math.ceil((width + Margin * 2) / cell));
        rows = Math.max(1, Math.ceil((height + Margin * 2) / cell));
        var size = columns * rows;
        if (cellHeads.length < size) cellHeads = new Int32Array(size);
        cellHeads.fill(-1, 0, size);
        if (nextInCell.length < nodes.length) nextInCell = new Int32Array(nodes.length + 64);
        for (var index = 0; index < nodes.length; index++) {
            var node = nodes[index];
            var column = Math.min(columns - 1, Math.max(0, ((node.x + Margin) / cell) | 0));
            var row = Math.min(rows - 1, Math.max(0, ((node.y + Margin) / cell) | 0));
            var key = row * columns + column;
            nextInCell[index] = cellHeads[key];
            cellHeads[key] = index;
        }
    }

    function link(first, second, limitSquared) {
        var differenceX = first.x - second.x, differenceY = first.y - second.y;
        var distanceSquared = differenceX * differenceX + differenceY * differenceY;
        if (distanceSquared >= limitSquared) return;
        var strength = (1 - Math.sqrt(distanceSquared) / linkDistance) * Math.min(first.alpha, second.alpha);
        if (strength <= 0.01) return;
        linkSegments[Math.min(Levels - 1, (strength * Levels) | 0)].push(first.x, first.y, second.x, second.y);
    }

    // Each pair once: later nodes in the same cell, and the four cells ahead.
    function collectLinks() {
        linkSegments.forEach(function (segments) { segments.length = 0; });
        var limitSquared = linkDistance * linkDistance;
        for (var row = 0; row < rows; row++) {
            for (var column = 0; column < columns; column++) {
                for (var index = cellHeads[row * columns + column]; index !== -1; index = nextInCell[index]) {
                    var node = nodes[index], other;
                    for (other = nextInCell[index]; other !== -1; other = nextInCell[other]) link(node, nodes[other], limitSquared);
                    if (column + 1 < columns) {
                        for (other = cellHeads[row * columns + column + 1]; other !== -1; other = nextInCell[other]) link(node, nodes[other], limitSquared);
                    }
                    if (row + 1 < rows) {
                        var below = (row + 1) * columns;
                        if (column > 0) {
                            for (other = cellHeads[below + column - 1]; other !== -1; other = nextInCell[other]) link(node, nodes[other], limitSquared);
                        }
                        for (other = cellHeads[below + column]; other !== -1; other = nextInCell[other]) link(node, nodes[other], limitSquared);
                        if (column + 1 < columns) {
                            for (other = cellHeads[below + column + 1]; other !== -1; other = nextInCell[other]) link(node, nodes[other], limitSquared);
                        }
                    }
                }
            }
        }
    }

    function strokeLevels(segmentsByLevel, color, peak, lineWidth) {
        context.lineWidth = lineWidth;
        for (var level = 0; level < Levels; level++) {
            var segments = segmentsByLevel[level];
            if (!segments.length) continue;
            context.strokeStyle = "rgba(" + color + "," + (((level + 0.5) / Levels) * peak).toFixed(3) + ")";
            context.beginPath();
            for (var index = 0; index < segments.length; index += 4) {
                context.moveTo(segments[index], segments[index + 1]);
                context.lineTo(segments[index + 2], segments[index + 3]);
            }
            context.stroke();
        }
    }

    /* Movement */

    function burst(x, y, big) {
        waves.push({ x: x, y: y, radius: 6, alpha: 0.55, largest: big ? 480 : 320, big: big });
        var count = big ? 40 : 24;
        for (var index = 0; index < count; index++) {
            var angle = Math.random() * Math.PI * 2;
            var speed = 1.4 + Math.random() * (big ? 6.5 : 4.5);
            sparks.push({ x: x, y: y, velocityX: Math.cos(angle) * speed, velocityY: Math.sin(angle) * speed, radius: 0.9 + Math.random() * 2.2, color: (Math.random() * 6) | 0, life: 0.85 + Math.random() * 0.6, fade: 0.018 });
        }
    }

    function step(frames) {
        simulationTime += frames / 60;
        var settle = 1 - Math.pow(0.975, frames); // back toward each node's own drift
        var reach = grabDistance, reachSquared = reach * reach;
        var pointerOn = pointer.active;
        var pointerSpeed = Math.min(Math.hypot(pointer.velocityX, pointer.velocityY), 30);

        nodes.forEach(function (node) {
            if (node.alpha < 1) node.alpha = Math.min(1, node.alpha + 0.025 * frames);
            node.twinkle += node.twinkleSpeed * frames;

            // A slow current, so the field never looks still.
            node.velocityX += Math.sin(node.y * 0.008 + simulationTime * 0.35) * 0.0035 * frames;
            node.velocityY += Math.cos(node.x * 0.008 + simulationTime * 0.35) * 0.0035 * frames;

            if (pointerOn) {
                var awayX = node.x - pointer.x, awayY = node.y - pointer.y;
                var distanceSquared = awayX * awayX + awayY * awayY;
                if (distanceSquared < reachSquared && distanceSquared > 0.25) {
                    var distance = Math.sqrt(distanceSquared), unitX = awayX / distance, unitY = awayY / distance;
                    var falloff = 1 - distance / reach; // 1 at the pointer, 0 at the edge of its reach
                    if (pointer.down) {
                        // Held down: a pull, softer near the middle so nodes circle instead of piling up.
                        var pull = falloff * 0.15 * Math.min(1, distance / 28);
                        node.velocityX -= unitX * pull * frames;
                        node.velocityY -= unitY * pull * frames;
                    } else {
                        // Hovering: a push that grows with the pointer's speed, and a little drag along its path.
                        var push = falloff * falloff * (0.08 + pointerSpeed * 0.012);
                        node.velocityX += (unitX * push + pointer.velocityX * falloff * 0.018 - unitY * falloff * 0.01) * frames;
                        node.velocityY += (unitY * push + pointer.velocityY * falloff * 0.018 + unitX * falloff * 0.01) * frames;
                    }
                }
            }

            waves.forEach(function (wave) {
                var outX = node.x - wave.x, outY = node.y - wave.y;
                var distance = Math.hypot(outX, outY);
                var offset = Math.abs(distance - wave.radius);
                if (offset < 56 && distance > 1) {
                    var force = (1 - offset / 56) * 1.9 * (wave.alpha / 0.55) * frames;
                    node.velocityX += (outX / distance) * force;
                    node.velocityY += (outY / distance) * force;
                }
            });

            node.velocityX += (node.baseVelocityX - node.velocityX) * settle;
            node.velocityY += (node.baseVelocityY - node.velocityY) * settle;
            var speed = Math.hypot(node.velocityX, node.velocityY);
            if (speed > MaximumSpeed) {
                node.velocityX *= MaximumSpeed / speed;
                node.velocityY *= MaximumSpeed / speed;
            }
            node.x += node.velocityX * frames;
            node.y += node.velocityY * frames;

            if (node.x < -Margin) node.x += width + Margin * 2;
            else if (node.x > width + Margin) node.x -= width + Margin * 2;
            if (node.y < -Margin) node.y += height + Margin * 2;
            else if (node.y > height + Margin) node.y -= height + Margin * 2;
        });

        // A pointer that stops has no speed, though no event says so.
        var pointerSlowing = Math.pow(0.82, frames);
        pointer.velocityX *= pointerSlowing;
        pointer.velocityY *= pointerSlowing;

        var sparkSlowing = Math.pow(0.965, frames);
        sparks.forEach(function (spark) {
            spark.velocityX *= sparkSlowing;
            spark.velocityY *= sparkSlowing;
            spark.velocityY += 0.02 * frames;
            spark.x += spark.velocityX * frames;
            spark.y += spark.velocityY * frames;
            spark.life -= spark.fade * frames;
        });
        sparks = sparks.filter(function (spark) { return spark.life > 0; });

        waves.forEach(function (wave) {
            wave.radius += 9.5 * frames;
            wave.alpha *= Math.pow(0.945, frames);
        });
        waves = waves.filter(function (wave) { return wave.alpha > 0.02 && wave.radius < wave.largest; });
    }

    /* Drawing */

    function draw(animated) {
        context.clearRect(0, 0, width, height);
        if (!nodes.length) return;

        buildGrid();
        collectLinks();
        strokeLevels(linkSegments, palette.link, palette.linkAlpha, 1);

        if (animated && pointer.active) {
            grabSegments.forEach(function (segments) { segments.length = 0; });
            var reachSquared = grabDistance * grabDistance;
            nodes.forEach(function (node) {
                var awayX = node.x - pointer.x, awayY = node.y - pointer.y;
                var distanceSquared = awayX * awayX + awayY * awayY;
                if (distanceSquared >= reachSquared) return;
                var strength = (1 - Math.sqrt(distanceSquared) / grabDistance) * node.alpha;
                if (strength <= 0.01) return;
                grabSegments[Math.min(Levels - 1, (strength * Levels) | 0)].push(pointer.x, pointer.y, node.x, node.y);
            });
            strokeLevels(grabSegments, palette.grab, palette.grabAlpha, 1);
        }

        nodes.forEach(function (node) {
            context.globalAlpha = node.alpha * (animated ? 0.6 + Math.sin(node.twinkle) * 0.35 : 0.75);
            context.fillStyle = palette.dots[node.color];
            context.beginPath();
            context.arc(node.x, node.y, node.radius, 0, Math.PI * 2);
            context.fill();
        });

        sparks.forEach(function (spark) {
            context.globalAlpha = Math.min(1, spark.life);
            context.fillStyle = palette.dots[spark.color];
            context.beginPath();
            context.arc(spark.x, spark.y, spark.radius, 0, Math.PI * 2);
            context.fill();
        });
        context.globalAlpha = 1;

        waves.forEach(function (wave) {
            context.lineWidth = wave.big ? 2.5 : 2;
            context.strokeStyle = "rgba(" + palette.ring + "," + wave.alpha.toFixed(3) + ")";
            context.beginPath();
            context.arc(wave.x, wave.y, wave.radius, 0, Math.PI * 2);
            context.stroke();
            context.lineWidth = 1.5;
            context.strokeStyle = "rgba(" + palette.ringSecond + "," + (wave.alpha * 0.5).toFixed(3) + ")";
            context.beginPath();
            context.arc(wave.x, wave.y, wave.radius * 0.7, 0, Math.PI * 2);
            context.stroke();
        });
    }

    /* One animation frame at a time, paused while hidden or when motion is reduced */

    var frameRequest = 0, lastTime = 0;

    function running() {
        return !reducedMotion.matches && !document.hidden;
    }

    function frame(now) {
        frameRequest = 0;
        if (!running()) return;
        var frames = Math.min(Math.max((now - lastTime) / 16.667, 0.25), 3);
        lastTime = now;
        step(frames);
        draw(true);
        frameRequest = requestAnimationFrame(frame);
    }

    function play() {
        if (frameRequest || !running()) return;
        lastTime = performance.now();
        frameRequest = requestAnimationFrame(frame);
    }

    function pause() {
        if (frameRequest) cancelAnimationFrame(frameRequest);
        frameRequest = 0;
    }

    /* The pointer */

    function pointerPosition(event) {
        var box = canvas.getBoundingClientRect();
        return [event.clientX - box.left, event.clientY - box.top];
    }

    function release() {
        pointer.active = false;
        pointer.down = false;
        pointer.velocityX = pointer.velocityY = 0;
    }

    addEventListener("pointermove", function (event) {
        var position = pointerPosition(event), x = position[0], y = position[1];
        var time = event.timeStamp || performance.now();
        if (pointer.active && pointer.type === event.pointerType) {
            var gap = time - pointer.time;
            if (gap > 0 && gap < 120) {
                // Pixels per 60th of a second, smoothed a little for fast, noisy mice.
                var scale = 16.667 / Math.max(gap, 4);
                pointer.velocityX = pointer.velocityX * 0.35 + (x - pointer.x) * scale * 0.65;
                pointer.velocityY = pointer.velocityY * 0.35 + (y - pointer.y) * scale * 0.65;
                var speed = Math.hypot(pointer.velocityX, pointer.velocityY);
                if (speed > 60) {
                    pointer.velocityX *= 60 / speed;
                    pointer.velocityY *= 60 / speed;
                }
            }
        } else {
            pointer.velocityX = pointer.velocityY = 0;
        }
        pointer.x = x;
        pointer.y = y;
        pointer.time = time;
        pointer.type = event.pointerType;
        pointer.active = true;

        var currentSpeed = Math.hypot(pointer.velocityX, pointer.velocityY);
        if (running() && currentSpeed > 18 && sparks.length < 220) {
            sparks.push({ x: x, y: y, velocityX: -pointer.velocityX * 0.04 + (Math.random() - 0.5), velocityY: -pointer.velocityY * 0.04 + (Math.random() - 0.5), radius: 0.8 + Math.random() * 1.4, color: (Math.random() * 6) | 0, life: 0.55, fade: 0.022 });
        }
    }, { passive: true });

    addEventListener("pointerdown", function (event) {
        if (event.pointerType === "mouse" && event.button !== 0) return;
        var position = pointerPosition(event);
        pointer.x = position[0];
        pointer.y = position[1];
        pointer.time = event.timeStamp || performance.now();
        pointer.type = event.pointerType;
        pointer.active = true;
        pointer.down = true;
    }, { passive: true });

    addEventListener("pointerup", function (event) {
        pointer.down = false;
        if (event.pointerType !== "mouse") release(); // a lifted finger or pen is gone
    }, { passive: true });
    addEventListener("pointercancel", release, { passive: true }); // such as a touch that became a scroll
    root.addEventListener("mouseleave", release);
    document.addEventListener("pointerout", function (event) {
        if (!event.relatedTarget && event.pointerType === "mouse") release();
    });
    addEventListener("blur", release);

    addEventListener("click", function (event) {
        if (!running() || event.detail === 0) return; // a click from the keyboard has no real place
        if (event.target.closest && event.target.closest("input, textarea, select, label, [contenteditable], pre, code")) return;
        var position = pointerPosition(event);
        burst(position[0], position[1], event.detail >= 2);
    });

    /* The surroundings */

    function changeTheme() {
        palette = palettes[isDark() ? "dark" : "light"];
        if (!running()) draw(false);
    }

    new MutationObserver(changeTheme).observe(root, { attributes: true, attributeFilter: ["data-theme"] });
    darkQuery.addEventListener("change", changeTheme);

    reducedMotion.addEventListener("change", function () {
        if (reducedMotion.matches) {
            pause();
            sparks = [];
            waves = [];
            nodes.forEach(function (node) { node.alpha = 1; });
            draw(false);
        } else {
            play();
        }
    });

    document.addEventListener("visibilitychange", function () {
        if (document.hidden) pause();
        else play();
    });

    if ("ResizeObserver" in window) {
        new ResizeObserver(resize).observe(canvas);
    }
    // A new pixel ratio, from zooming or another monitor, does not resize the box.
    addEventListener("resize", resize);

    resize();
    play();
})();
