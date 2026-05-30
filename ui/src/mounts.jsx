/* ============================================================
   Doobie · React mounts: tape deck + digital VU meters
   ============================================================
   The TapeDeck wraps tape.js (the Space-Echo loop animation).
   DigitalMeter is driven by live dBFS values arriving via the
   "levels" native event from the engine -- proper 20·log10
   metering, RMS-bar + peak-hold + clip indicator, no fakery.
   ============================================================ */

/* ---- Tape deck (Space Echo loop) ---- */
function TapeDeck({ heads, playing, recording, speed = 1, accent = 'var(--accent)' }){
  const ref = React.useRef(null);
  const api = React.useRef(null);
  React.useEffect(() => {
    // Resolve CSS custom property at runtime so we can pass a real colour
    // to the tape script (its SVG strings need a concrete value).
    const acc = (accent === 'var(--accent)' || !accent)
      ? getComputedStyle(document.documentElement).getPropertyValue('--accent').trim() || '#e8863a'
      : accent;
    api.current = window.createTapeDeck(ref.current, { accent: acc });
    api.current.setHeads(heads);
    api.current.setPlaying(playing);
    api.current.setRecording(recording);
    api.current.setSpeed(speed);
    return () => { api.current && api.current.destroy(); };
  }, []);
  React.useEffect(() => { api.current && api.current.setHeads(heads); }, [heads]);
  React.useEffect(() => { api.current && api.current.setPlaying(playing); }, [playing]);
  React.useEffect(() => { api.current && api.current.setRecording(recording); }, [recording]);
  React.useEffect(() => { api.current && api.current.setSpeed(speed); }, [speed]);
  return <div className="tape-deck" ref={ref} />;
}

/* ---- Digital VU meter — RMS + peak-hold, dBFS, driven from engine ----
   The `liveDb` prop is a real peak-hold dB number arriving on the
   "levels" native event (~30 Hz from the editor). Smoothing the bar
   width keeps the visual readable even at coarser update rates. */
const DM_MIN = -54, DM_MAX = 3;
const dmPos = (db) => Math.max(0, Math.min(1, (db - DM_MIN) / (DM_MAX - DM_MIN))) * 100;

function DigitalMeter({ label, liveDb = -90, big = false, scale = false }){
  const mask = React.useRef(null), peak = React.useRef(null), read = React.useRef(null), clip = React.useRef(null);
  // smoothed bar position (one-pole follower on dB), independent of how
  // often the prop ticks
  const smooth = React.useRef({ rms: -90, last: performance.now() });
  React.useEffect(() => {
    let raf = 0;
    const tick = (now) => {
      const dt = Math.min(0.05, (now - smooth.current.last) / 1000); smooth.current.last = now;
      const target = isFinite(liveDb) ? liveDb : -90;
      // 80 ms attack, 200 ms release in the visual smoother
      const k = target > smooth.current.rms ? 1 - Math.exp(-dt / 0.08) : 1 - Math.exp(-dt / 0.20);
      smooth.current.rms += (target - smooth.current.rms) * k;
      if (mask.current){
        mask.current.style.left = dmPos(smooth.current.rms) + '%';
        peak.current.style.left = dmPos(target) + '%';
        peak.current.style.background = target >= 0 ? 'var(--peak)' : '#fff';
        read.current.textContent = target <= -53.5 ? '−∞' : (target > 0 ? '+' : '') + target.toFixed(1);
        clip.current.dataset.on = target >= 0 ? '1' : '0';
      }
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [liveDb]);
  const marks = [0, -6, -12, -24, -48];
  return (
    <div className={'dm' + (big ? ' big' : '')}>
      <div className="dm-hd">
        <span className="dm-nm">{label}</span>
        <span className="dm-read"><b ref={read}>{'−∞'}</b><i>dBFS</i></span>
      </div>
      <div className="dm-bar">
        <div className="dm-mask" ref={mask} />
        <div className="dm-zero" style={{ left: dmPos(0) + '%' }} />
        <div className="dm-peak" ref={peak} />
        <div className="dm-clip" ref={clip} data-on="0" />
      </div>
      {scale &&
        <div className="dm-scale">{marks.map(m => <span key={m} style={{ left: dmPos(m) + '%' }}>{m}</span>)}</div>}
    </div>
  );
}

Object.assign(window, { TapeDeck, DigitalMeter });
