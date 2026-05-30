/* ============================================================
   Doobie · visualizers — Reverb decay graph + LFO waveform mini
   (TapTimeline + TapeScreen + StereoMeters from the prototype
    are obsolete -- the cassette tape deck IS the timeline now,
    and DigitalMeter replaces the analog needles.)
   ============================================================ */

/* ---------- Reverb decay graph ---------- */
function DecayGraph({ decay = 0.6, type = 'Plate', height = 116 }){
  const W = 560, H = height;
  const k = 2.2 + (1 - decay) * 6;
  const pts = [];
  for (let i = 0; i <= 60; i++){
    const t = i / 60;
    const amp = Math.exp(-t * k);
    pts.push([t * W, H - 8 - amp * (H - 22)]);
  }
  const line = pts.map((p, i) => (i ? 'L' : 'M') + p[0].toFixed(1) + ' ' + p[1].toFixed(1)).join(' ');
  const area = `${line} L ${W} ${H} L 0 ${H} Z`;
  const secs = (3 + decay * 50).toFixed(1);
  return (
    <div className="graph" style={{ height: H }}>
      <div className="glabel">{type} &nbsp;decay <b>~{secs} s</b></div>
      <svg width="100%" height={H} viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none">
        <defs>
          <linearGradient id="decayFill" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%"  stopColor="rgba(244,239,230,.42)" />
            <stop offset="60%" stopColor="rgba(244,239,230,.10)" />
            <stop offset="100%" stopColor="rgba(244,239,230,0)" />
          </linearGradient>
        </defs>
        {[0.2,0.4,0.6,0.8].map(g => <line key={g} x1={g*W} y1="6" x2={g*W} y2={H} stroke="rgba(255,255,255,.05)" strokeWidth="1" />)}
        <path d={area} fill="url(#decayFill)" />
        <path d={line} fill="none" stroke="rgba(255,255,255,.6)" strokeWidth="1.4" />
        <rect x="0" y={H - 8 - (H-22)} width="5" height={H-22+8} rx="2" fill="var(--accent)" />
      </svg>
    </div>
  );
}

/* ---------- LFO waveform mini ---------- */
function WaveMini({ shape = 'Sine', rate = 0.4, depth = 0.6 }){
  const W = 120, H = 40, mid = H/2, amp = (H/2 - 4) * Math.max(0.15, depth);
  const cycles = 2;
  const fn = (p) => {
    const x = (p % 1);
    switch(shape){
      case 'Triangle': return x < 0.5 ? (1 - 4*x) : (-3 + 4*x);
      case 'Square':   return x < 0.5 ? 1 : -1;
      case 'Saw Up':   return -1 + 2*x;
      case 'Saw Down': return 1 - 2*x;
      case 'Random':
      case 'Random S&H': return Math.sin(p*7.3)*0.5 + Math.sin(p*3.1)*0.5;
      default:         return Math.sin(p * Math.PI * 2);
    }
  };
  let d = '';
  const N = 120;
  for (let i = 0; i <= N; i++){
    const px = (i / N) * W * 2;
    const p  = (i / N) * cycles * 2;
    const py = mid - fn(p) * amp;
    d += (i ? 'L' : 'M') + px.toFixed(1) + ' ' + py.toFixed(1) + ' ';
  }
  const dur = Math.max(0.6, 4.5 - rate * 3.5).toFixed(2);
  return (
    <div className="wave">
      <svg width="100%" height={H} viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none">
        <line x1="0" y1={mid} x2={W} y2={mid} stroke="var(--c-line-2)" strokeWidth="0.5" />
        <g className="wave-scroll" style={{ animationDuration: dur + 's', transformOrigin: 'center' }}>
          <path d={d} fill="none" stroke="var(--accent)" strokeWidth="1.6" />
        </g>
      </svg>
    </div>
  );
}

Object.assign(window, { DecayGraph, WaveMini });

/* ---------- Reusable modal (preset save, future dialogs) ----------
   The host's native AlertWindow looks out of place in a WebView UI;
   this is the same dialog drawn in the plugin's own language. */
function Modal({ open, title, message, defaultValue = '', onConfirm, onCancel, confirmLabel = 'Save' }){
  const ref = React.useRef(null);
  const [v, setV] = React.useState(defaultValue);
  React.useEffect(() => { if (open) { setV(defaultValue); setTimeout(() => ref.current && ref.current.focus(), 50); } }, [open, defaultValue]);
  React.useEffect(() => {
    if (!open) return;
    const onKey = (e) => {
      if (e.key === 'Escape') onCancel && onCancel();
      if (e.key === 'Enter' && v.trim()) onConfirm && onConfirm(v.trim());
    };
    document.addEventListener('keydown', onKey);
    return () => document.removeEventListener('keydown', onKey);
  }, [open, v, onConfirm, onCancel]);
  return (
    <div className={'modal-scrim' + (open ? ' open' : '')} onMouseDown={(e) => { if (e.target.classList.contains('modal-scrim')) onCancel && onCancel(); }}>
      <div className="modal" role="dialog" aria-modal="true">
        <h3>{title}</h3>
        {message && <p>{message}</p>}
        <input type="text" ref={ref} value={v} onChange={(e) => setV(e.target.value)} />
        <div className="modal-actions">
          <button className="btn ghost"  onClick={() => onCancel && onCancel()}>Cancel</button>
          <button className="btn accent" onClick={() => v.trim() && onConfirm && onConfirm(v.trim())} disabled={!v.trim()}>{confirmLabel}</button>
        </div>
      </div>
    </div>
  );
}

Object.assign(window, { Modal });

/* ---------- Knob right-click context menu ----------
   Singleton, mounted by App, opened by `window.openKnobMenu(x, y, opts)`. */
let _kbOpenerSetter = null;
window.openKnobMenu = (x, y, opts) => { if (_kbOpenerSetter) _kbOpenerSetter({ x, y, opts }); };

function parseValue(text, format, currentValue){
  // Heuristic: invert the format function. We don't have inverse functions
  // for every formatter so we just store the raw fraction the user types
  // when the value contains "%", and the raw number when it doesn't.
  const t = (text || '').trim();
  if (t.endsWith('%')) return Math.max(0, Math.min(1, parseFloat(t) / 100));
  const n = parseFloat(t);
  if (isFinite(n)) {
    // Allow 0..1 normalized entry directly (common case for "raw" knobs)
    if (n >= 0 && n <= 1) return n;
  }
  return currentValue;
}

function KnobContextMenu(){
  const [s, setS] = React.useState(null);
  const [edit, setEdit] = React.useState(null);
  React.useEffect(() => { _kbOpenerSetter = setS; return () => { _kbOpenerSetter = null; }; }, []);
  React.useEffect(() => {
    if (!s) return;
    const close = () => setS(null);
    document.addEventListener('mousedown', close, { once: true });
    return () => document.removeEventListener('mousedown', close);
  }, [s]);

  if (edit) {
    return (
      <Modal open={true} title={'Set ' + (edit.opts.label || 'value')}
             message={'Current: ' + (edit.opts.format ? edit.opts.format (edit.opts.value) : Math.round (edit.opts.value * 100) + '%')
                      + '   Enter a number 0–1 or a percentage like 75%.'}
             defaultValue={edit.opts.format ? edit.opts.format (edit.opts.value) : (edit.opts.value).toFixed(3)}
             onConfirm={(t) => { edit.opts.onChange (parseValue (t, edit.opts.format, edit.opts.value)); setEdit(null); }}
             onCancel={() => setEdit(null)} confirmLabel="Set" />
    );
  }

  if (!s) return null;
  const o = s.opts;
  const valTxt = o.format ? o.format(o.value) : (Math.round(o.value*100) + '%');
  const item = (label, action) => (
    <button className="kbmenu-item" onMouseDown={(e) => { e.stopPropagation(); action(); setS(null); }}>{label}</button>
  );
  // Clamp menu position so it doesn't fly off-screen.
  const W = 200, H = 170;
  const x = Math.min(s.x, window.innerWidth - W - 8);
  const y = Math.min(s.y, window.innerHeight - H - 8);
  return (
    <div className="kbmenu" style={{ left: x, top: y }} onMouseDown={(e) => e.stopPropagation()}>
      <div className="kbmenu-hd"><span className="kbmenu-lab">{o.label || 'Value'}</span><span className="kbmenu-val">{valTxt}</span></div>
      {item ('Reset to default',     () => o.onChange (o.defaultValue))}
      {item ('Copy value',           () => { navigator.clipboard && navigator.clipboard.writeText (valTxt); })}
      {item ('Paste value',          async () => {
        if (!navigator.clipboard) return;
        const t = await navigator.clipboard.readText();
        o.onChange (parseValue (t, o.format, o.value));
      })}
      {item ('Enter value…',    () => setEdit ({ opts: o }))}
    </div>
  );
}

Object.assign(window, { KnobContextMenu, parseValue });
