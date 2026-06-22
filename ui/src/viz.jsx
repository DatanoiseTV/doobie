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
function WaveMini({ shape = 'Sine', rate = 0.4, depth = 0.6, value = null }){
  const W = 120, H = 40, mid = H/2, amp = (H/2 - 4) * Math.max(0.15, depth);
  const cycles = 2;
  // Static shape preview — drawn as a single SVG path so the user sees the
  // characteristic of each waveform (square is flat-flat, triangle has
  // straight ramps, saw teeth, S&H is jagged). Triangle is rebuilt to a
  // canonical 0→+1→0→-1→0 shape so it's recognisable at a glance.
  const fn = (p) => {
    const x = (p % 1 + 1) % 1; // keep p positive even when called via value samples
    switch(shape){
      case 'Triangle': {
        if (x < 0.25) return 4 * x;
        if (x < 0.75) return 2 - 4 * x;
        return -4 + 4 * x;
      }
      case 'Square':   return x < 0.5 ? 1 : -1;
      case 'Saw Up':   return -1 + 2 * x;
      case 'Saw Down': return 1 - 2 * x;
      case 'Random':
      case 'Random S&H': {
        // Deterministic pseudo-random pulse train for the static preview
        // — same x bins yield the same value each render so the path is
        // stable instead of crawling.
        const bin = Math.floor (x * 8);
        return (Math.sin (bin * 12.9898) * 43758.5453) % 1 * 2 - 1;
      }
      default: return Math.sin (x * Math.PI * 2);
    }
  };
  let d = '';
  const N = 240;
  for (let i = 0; i <= N; i++){
    const px = (i / N) * W * 2;
    const p  = (i / N) * cycles;
    const py = mid - fn(p) * amp;
    d += (i ? 'L' : 'M') + px.toFixed(1) + ' ' + py.toFixed(1) + ' ';
  }
  const dur = Math.max(0.6, 4.5 - rate * 3.5).toFixed(2);

  // Live-value indicator: when the caller passes the LFO's current value
  // (post-depth, in [-1, +1]) we plot it as a moving dot on the centre
  // line. The dot's Y motion is the actual waveform shape — sine moves
  // smoothly, triangle linearly, square JUMPs, S&H teleports. Without
  // this the user only saw the static shape and the CSS scroll motion
  // (which looks the same regardless of wave).
  const liveOn = value != null && isFinite (value);
  const liveY  = liveOn ? (mid - Math.max(-1, Math.min(1, value)) * amp) : mid;
  return (
    <div className="wave">
      <svg width="100%" height={H} viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none">
        <line x1="0" y1={mid} x2={W} y2={mid} stroke="var(--c-line-2)" strokeWidth="0.5" />
        <g className="wave-scroll" style={{ animationDuration: dur + 's', transformOrigin: 'center' }}>
          <path d={d} fill="none" stroke="var(--accent-dim, var(--accent))" strokeWidth="1.4" opacity="0.55" />
        </g>
        {liveOn && (
          <>
            <line x1={W/2} y1={mid} x2={W/2} y2={liveY} stroke="var(--accent)" strokeWidth="1" opacity="0.4" />
            <circle cx={W/2} cy={liveY} r="3" fill="var(--accent)" />
          </>
        )}
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

/* ============================================================
   PresetBrowser — fullscreen-ish modal listing the factory bank
   ============================================================
   The C++ side ships the name list via Juce.getNativeFunction
   ('listFactoryPresets'). We derive the same simple category
   tag the WebEditor uses for the header so the rows match the
   chip shown after load — that mapping lives in one place
   conceptually even if it's evaluated twice (C++ + JS). When the
   manager grows a real categoryOf() it'll feed both sides. */
const PRESET_CATS = ['ALL', 'USER', 'DUB', 'AMBIENT', 'VINTAGE', 'WIDE', 'OTHER'];
function categoryOf (name, isUser) {
  if (isUser) return 'USER';
  if (!name) return 'OTHER';
  if (/Dub|Reggae/i.test(name)) return 'DUB';
  if (/Ambient/i.test(name))    return 'AMBIENT';
  if (/Vintage/i.test(name))    return 'VINTAGE';
  if (/Cosmic/i.test(name))     return 'WIDE';
  return 'OTHER';
}

function PresetBrowser ({ open, onClose, currentName }) {
  const [names, setNames]   = React.useState([]);
  const [query, setQuery]   = React.useState('');
  const [cat,   setCat]     = React.useState('ALL');
  const inputRef = React.useRef(null);

  // Pull both lists each time the modal opens. Names are de-duped favouring
  // the user version when a name collides (a user save with the same name
  // as a factory shadows the factory — that's the existing engine
  // behaviour in loadByName, so the browser matches it).
  React.useEffect(() => {
    if (!open) return;
    setQuery('');
    let cancelled = false;
    const fac  = window.Juce.getNativeFunction('listFactoryPresets');
    const user = window.Juce.getNativeFunction('listUserPresets');
    Promise.all([Promise.resolve(fac()), Promise.resolve(user())]).then(([facList, userList]) => {
      if (cancelled) return;
      const facArr  = Array.isArray(facList)  ? facList  : [];
      const userArr = Array.isArray(userList) ? userList : [];
      const userSet = new Set (userArr);
      const merged  = [
        ...userArr.map (n => ({ name: n, isUser: true })),
        ...facArr.filter (n => !userSet.has (n)).map (n => ({ name: n, isUser: false })),
      ];
      setNames (merged);
    });
    setTimeout(() => { if (inputRef.current) inputRef.current.focus(); }, 60);
    return () => { cancelled = true; };
  }, [open]);

  React.useEffect(() => {
    if (!open) return;
    const onKey = (e) => { if (e.key === 'Escape') onClose && onClose(); };
    document.addEventListener('keydown', onKey);
    return () => document.removeEventListener('keydown', onKey);
  }, [open, onClose]);

  if (!open) return null;
  const q = query.trim().toLowerCase();
  // `names` is now [{name, isUser}]; build the display rows with the
  // computed cat tag (USER overrides factory heuristics so a user save
  // named "Vintage Whatever" still shows up under USER not VINTAGE).
  const rows = names
    .map(entry => ({ name: entry.name, isUser: entry.isUser, cat: categoryOf(entry.name, entry.isUser) }))
    .filter(r => (cat === 'ALL' || r.cat === cat) && (!q || r.name.toLowerCase().includes(q)));

  // Stays open after a row is clicked so the user can audition through the
  // bank without reopening the modal each time. The selected row stays
  // highlighted (currentName updates via the presetInfo event) and Close /
  // Esc dismiss when they're done.
  const choose = (n) => {
    window.Juce.backend.emitEvent('preset_load', { name: n });
  };

  return (
    <div className="modal-scrim open" onMouseDown={(e) => { if (e.target.classList.contains('modal-scrim')) onClose && onClose(); }}>
      <div className="modal browser" role="dialog" aria-modal="true">
        <h3>Presets</h3>
        <div className="br-head">
          <input ref={inputRef} className="br-search" placeholder="Search..." value={query}
                 onChange={(e) => setQuery(e.target.value)} />
        </div>
        <div className="br-cats">
          {PRESET_CATS.map(c =>
            <button key={c} data-on={cat === c ? '1' : '0'} onClick={() => setCat(c)}>{c}</button>
          )}
        </div>
        <div className="br-list">
          {rows.length === 0
            ? <div className="br-empty">No presets match.</div>
            : rows.map(r =>
                <div key={(r.isUser ? 'u:' : 'f:') + r.name} className="br-row"
                     data-on={r.name === currentName ? '1' : '0'}
                     onClick={() => choose(r.name)}>
                  <span>{r.name}</span>
                  <span className="cat">{r.cat}</span>
                </div>
              )}
        </div>
        <div className="br-foot">
          <span className="count">{rows.length} / {names.length}</span>
          <span className="spacer" />
          <button className="btn ghost" onClick={() => onClose && onClose()}>Close</button>
        </div>
      </div>
    </div>
  );
}

/* ============================================================
   IRPicker — used by the Convolution reverb mode
   ============================================================ */
function IRPicker ({ irInfo }) {
  const [browserOpen, setBrowserOpen] = React.useState(false);
  const [names, setNames] = React.useState([]);
  const [query, setQuery] = React.useState('');

  // Lazy-load the IR name list. The factory list is static so a one-shot is
  // enough; we refresh when the modal opens in case the C++ side ever grows
  // a user-IR bank later.
  React.useEffect(() => {
    if (!browserOpen) return;
    setQuery('');
    let cancelled = false;
    Promise.resolve(window.Juce.getNativeFunction('listFactoryIRs')()).then(list => {
      if (!cancelled) setNames(Array.isArray(list) ? list : []);
    });
    return () => { cancelled = true; };
  }, [browserOpen]);

  const has    = !!(irInfo && irInfo.hasIR);
  const isFac  = !!(irInfo && irInfo.isFactory);
  const isFile = !!(irInfo && irInfo.isFile);
  const name   = (irInfo && irInfo.name) || (has ? 'Impulse Response' : '(no IR)');

  const pickFactory = (idx) => {
    window.Juce.backend.emitEvent('ir_load_factory', { index: idx });
    setBrowserOpen(false);
  };
  const pickFile = () => window.Juce.backend.emitEvent('ir_load_file', {});
  const clear    = () => window.Juce.backend.emitEvent('ir_clear', {});

  const q = query.trim().toLowerCase();
  const rows = names.map((n, i) => ({ name: n, idx: i }))
                    .filter(r => !q || r.name.toLowerCase().includes(q));

  return (
    <>
      <div className="ir-pick">
        <span className="ir-tag">{isFile ? 'FILE' : isFac ? 'FACTORY' : 'OFF'}</span>
        <span className="ir-name" title={name}>{name}</span>
        <button className="ir-btn" onClick={() => setBrowserOpen(true)}>Browse</button>
        <button className="ir-btn" onClick={pickFile}>Load file...</button>
        {has && <button className="ir-btn" onClick={clear}>Clear</button>}
      </div>
      {browserOpen && (
        <div className="modal-scrim open" onMouseDown={(e) => { if (e.target.classList.contains('modal-scrim')) setBrowserOpen(false); }}>
          <div className="modal browser" role="dialog" aria-modal="true">
            <h3>Impulse Responses</h3>
            <div className="br-head">
              <input className="br-search" placeholder="Search..." value={query}
                     onChange={(e) => setQuery(e.target.value)} autoFocus />
            </div>
            <div className="br-list">
              {rows.length === 0
                ? <div className="br-empty">No IRs match.</div>
                : rows.map(r =>
                    <div key={r.idx} className="br-row"
                         data-on={isFac && r.idx === irInfo.factoryIndex ? '1' : '0'}
                         onClick={() => pickFactory(r.idx)}>
                      <span>{r.name}</span>
                      <span className="cat">#{r.idx + 1}</span>
                    </div>
                  )}
            </div>
            <div className="br-foot">
              <span className="count">{rows.length} / {names.length}</span>
              <span className="spacer" />
              <button className="btn ghost"  onClick={() => setBrowserOpen(false)}>Close</button>
            </div>
          </div>
        </div>
      )}
    </>
  );
}

Object.assign(window, { PresetBrowser, IRPicker });
