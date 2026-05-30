/* ============================================================
   Space-Echo-style tape LOOP — single continuous loop circulating
   past a multi-head block (record + playback heads A–D) and a
   capstan + pinch roller. Constant linear tape speed everywhere
   (uniform dash motion) — physically correct for a loop echo.
   Playback heads slide along the loop by their Time value.
   window.createTapeDeck(container, opts) -> API
   ============================================================ */
(function () {
  const SVGNS = 'http://www.w3.org/2000/svg';

  window.createTapeDeck = function (container, opts) {
    const o = opts || {};
    const ink = o.ink || '#e9e4da';
    const accent = o.accent || '#e8863a';

    /* loop geometry */
    const TOP_Y = 126, BOT_Y = 232;
    const LEFT_X = 116, RIGHT_X = 574, RAD = 16;
    const REC_X = 172, PB_X0 = 236, PB_X1 = 484, CAP_X = 520;
    const ROLLERS = [[132, 142], [558, 142], [132, 216], [558, 216]];

    // closed loop centreline (clockwise: bottom L→R, up R, top R→L with slack belly, down L)
    const LOOP =
      `M 132 ${BOT_Y} L 504 ${BOT_Y} L 558 ${BOT_Y} Q ${RIGHT_X} ${BOT_Y} ${RIGHT_X} ${BOT_Y - RAD} ` +
      `L ${RIGHT_X} ${TOP_Y + RAD} Q ${RIGHT_X} ${TOP_Y} 558 ${TOP_Y} ` +
      `Q 345 ${TOP_Y + 44} 132 ${TOP_Y} Q ${LEFT_X} ${TOP_Y} ${LEFT_X} ${TOP_Y + RAD} ` +
      `L ${LEFT_X} ${BOT_Y - RAD} Q ${LEFT_X} ${BOT_Y} 132 ${BOT_Y} Z`;

    const state = { playing: true, recording: true, speed: 1, heads: [] };

    const rollerSVG = (id, cx, cy) => `
      <g transform="translate(${cx},${cy})">
        <circle r="16" stroke="currentColor" stroke-width="1.3" fill="#0c0b08"/>
        <g id="${id}">
          <g stroke="currentColor" stroke-width="1.1">
            <line x1="0" y1="0" x2="0" y2="-11"/><line x1="0" y1="0" x2="9.5" y2="5.5"/><line x1="0" y1="0" x2="-9.5" y2="5.5"/>
          </g>
          <circle r="5.5" stroke="currentColor" stroke-width="1" fill="#000"/>
          <circle r="1.6" fill="currentColor" stroke="none"/>
        </g>
      </g>`;

    container.style.color = ink;
    container.innerHTML = `
      <svg id="tapesvg" viewBox="0 110 680 168" shape-rendering="geometricPrecision" style="display:block;width:100%;height:100%"
           font-family="'Helvetica Neue',Arial,sans-serif" fill="none">
        ${rollerSVG('rh0', ROLLERS[0][0], ROLLERS[0][1])}
        ${rollerSVG('rh1', ROLLERS[1][0], ROLLERS[1][1])}
        ${rollerSVG('rh2', ROLLERS[2][0], ROLLERS[2][1])}
        ${rollerSVG('rh3', ROLLERS[3][0], ROLLERS[3][1])}
        <path id="tline" d="${LOOP}" stroke="currentColor" stroke-width="3.2" fill="none" opacity="0.5"/>
        <path id="tanim" d="${LOOP}" stroke="${accent}" stroke-width="1.7" fill="none" stroke-dasharray="4 12" opacity="0.9"/>
        <g id="baseplate"></g>
        <g id="heads"></g>
        <!-- capstan + pinch -->
        <g transform="translate(${CAP_X},${BOT_Y + 14})">
          <circle r="9.5" stroke="currentColor" stroke-width="1.4" fill="#000"/>
          <circle r="1.5" fill="currentColor" stroke="none"/>
        </g>
        <g id="capstan" transform="translate(${CAP_X},${BOT_Y})">
          <circle r="4.2" fill="currentColor" stroke="none"/>
          <g id="cap-hub" stroke="#000" stroke-width="0.9"><line x1="-3" y1="0" x2="3" y2="0"/><line x1="0" y1="-3" x2="0" y2="3"/></g>
        </g>
      </svg>`;

    const $ = (id) => container.querySelector('#' + id);
    const el = {
      anim: $('tanim'), heads: $('heads'), base: $('baseplate'),
      hubs: ['rh0', 'rh1', 'rh2', 'rh3'].map((i) => $(i)), cap: $('cap-hub'),
    };
    let dashOff = 0, rot = 0, last = performance.now(), raf = 0, alive = true;

    function activeHeads() {
      return state.heads.filter(h => h.on)
        .map(h => ({ x: PB_X0 + h.time * (PB_X1 - PB_X0), id: h.id })).sort((a, b) => a.x - b.x);
    }
    function renderHeads() {
      const list = [{ x: REC_X, id: 'R', rec: true }, ...activeHeads()];
      const xs = list.map(h => h.x);
      const x0 = Math.min(...xs) - 12, x1 = Math.max(...xs) + 12;
      el.base.innerHTML = `<rect x="${x0.toFixed(1)}" y="${BOT_Y + 5}" width="${(x1 - x0).toFixed(1)}" height="15" rx="3" stroke="currentColor" stroke-width="1.2" fill="#0a0908" opacity="0.9"/>`;
      const g = el.heads; while (g.firstChild) g.removeChild(g.firstChild);
      for (const h of list) {
        const col = h.rec ? accent : ink;
        const grp = document.createElementNS(SVGNS, 'g');
        grp.setAttribute('transform', `translate(${h.x.toFixed(1)},0)`);
        grp.innerHTML =
          `<rect x="-6.5" y="${BOT_Y - 4}" width="13" height="22" rx="2" stroke="${col}" stroke-width="1.3" fill="#000"/>` +
          `<line x1="0" y1="${BOT_Y - 4}" x2="0" y2="${BOT_Y + 5}" stroke="${col}" stroke-width="1.1" opacity="0.85"/>` +
          `<circle cx="0" cy="${BOT_Y - 7}" r="2" fill="${col}" stroke="none" class="tape-tap"/>` +
          `<text x="0" y="${BOT_Y + 36}" text-anchor="middle" font-size="12" fill="${col}" stroke="none" font-weight="600" letter-spacing="0.5">${h.id}</text>`;
        g.appendChild(grp);
      }
    }

    function tick(now) {
      if (!alive) return;
      const dt = (now - last) / 1000; last = now;
      if (state.playing) {
        dashOff -= 70 * state.speed * dt;       // uniform linear tape speed
        el.anim.setAttribute('stroke-dashoffset', dashOff.toFixed(1));
        rot = (rot - 150 * state.speed * dt) % 360;   // all rollers turn with the loop
        el.hubs.forEach((h) => h && h.setAttribute('transform', `rotate(${rot.toFixed(1)})`));
        el.cap && el.cap.setAttribute('transform', `rotate(${(rot * 2.4).toFixed(1)})`);
      }
      raf = requestAnimationFrame(tick);
    }
    renderHeads(); raf = requestAnimationFrame(tick);

    return {
      setHeads(arr) { state.heads = arr.map(h => ({ id: h.id, on: !!h.on, time: h.time, level: h.level })); renderHeads(); },
      setPlaying(b) { state.playing = !!b; },
      setRecording(b) { state.recording = !!b; },
      setSpeed(s) { state.speed = s; },
      destroy() { alive = false; cancelAnimationFrame(raf); },
    };
  };
})();
