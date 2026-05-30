/* ============================================================
   DOOBIE · interactive atoms — Knob, Chip, Toggle, Selector
   ============================================================ */

/* ---- geometry helpers ---- */
function polar(cx, cy, r, deg){
  const rad = (deg) * Math.PI / 180;
  return [cx + r * Math.sin(rad), cy - r * Math.cos(rad)];
}
function arcPath(cx, cy, r, a0, a1){
  const [x0, y0] = polar(cx, cy, r, a0);
  const [x1, y1] = polar(cx, cy, r, a1);
  const large = (a1 - a0) > 180 ? 1 : 0;
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

const A0 = -135, A1 = 135, SWEEP = A1 - A0;

/* ---- Knob ---- */
function Knob({ value = 0.5, onChange, size = 'md', label, format, lit = false, bipolar = false, mod = 0, arcColor, defaultValue }){
  const ref = useRef(null);
  const [drag, setDrag] = useState(false);
  const stash = useRef({ y: 0, v: 0 });

  // Right-click opens a small contextual menu — Reset / Copy / Paste / Enter
  // value. Industry-standard knob convention. Browser context menu is
  // suppressed app-wide; this is the per-control replacement.
  const onCtx = (e) => {
    e.preventDefault();
    if (window.openKnobMenu)
      window.openKnobMenu (e.clientX, e.clientY, {
        value,
        defaultValue: typeof defaultValue === 'number' ? defaultValue : (bipolar ? 0.5 : 0),
        format,
        onChange,
        label: label || '',
      });
  };

  const D = size === 'lg' ? 116 : size === 'sm' ? 46 : 60;
  const sw = size === 'lg' ? 4 : size === 'sm' ? 2.6 : 3.2;
  const c = D / 2;
  const R = c - sw - 2;
  const ang = A0 + value * SWEEP;
  const midAng = 0; // top, for bipolar fill origin
  const bodyR = R - (size === 'lg' ? 11 : size === 'sm' ? 6 : 8);

  const [px, py] = polar(c, c, R, ang);
  const pointerInner = bodyR - (size === 'lg' ? 10 : 4);
  const [pix, piy] = polar(c, c, pointerInner, ang);

  const onDown = useCallback((e) => {
    e.preventDefault();
    const pt = e.touches ? e.touches[0] : e;
    stash.current = { y: pt.clientY, v: value };
    setDrag(true);
    const move = (ev) => {
      const p = ev.touches ? ev.touches[0] : ev;
      const fine = ev.shiftKey ? 0.25 : 1;
      const dv = (stash.current.y - p.clientY) / 240 * fine;
      let nv = Math.max(0, Math.min(1, stash.current.v + dv));
      onChange && onChange(nv);
    };
    const up = () => {
      setDrag(false);
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }, [value, onChange]);

  const onDbl = () => onChange && onChange(bipolar ? 0.5 : 0.5);

  const ticks = [];
  const tickN = size === 'sm' ? 0 : 7;
  for (let i = 0; i < tickN; i++){
    const a = A0 + (i / (tickN - 1)) * SWEEP;
    const [x1, y1] = polar(c, c, R + 3.5, a);
    const [x2, y2] = polar(c, c, R + 6.5, a);
    ticks.push(<line key={i} className="k-tick" x1={x1} y1={y1} x2={x2} y2={y2} strokeWidth="1" />);
  }

  const arc = bipolar
    ? (value >= 0.5 ? arcPath(c, c, R, midAng, ang) : arcPath(c, c, R, ang, midAng))
    : arcPath(c, c, R, A0, ang);

  // modulation range + animated indicator (mod = normalized half-range)
  const modOn = mod > 0.004;
  const Rm = R + (size === 'sm' ? 4 : 8.5);
  let modPath = '';
  if (modOn){
    const lo = Math.max(0, Math.min(1, value - mod)), hi = Math.max(0, Math.min(1, value + mod));
    modPath = arcPath(c, c, Rm, A0 + lo * SWEEP, A0 + hi * SWEEP);
  }
  const arcStyle = arcColor ? { stroke: arcColor, filter: `drop-shadow(0 0 5px ${arcColor})` } : undefined;

  const valTxt = format ? format(value) : Math.round(value * 100) + '%';

  return (
    <div className={'knob' + (lit ? ' lit' : '')}>
      <div ref={ref} className={'dial' + (drag ? ' dragging' : '')}
           onPointerDown={onDown} onDoubleClick={onDbl} onContextMenu={onCtx}
           style={{ width: D, height: D }}>
        <div className="kval">{valTxt}</div>
        <svg width={D} height={D} viewBox={`0 0 ${D} ${D}`}>
          {ticks}
          {modOn && <path className="k-modrange" d={modPath} fill="none" strokeWidth="1.8" strokeLinecap="round" />}
          <path className="k-track" d={arcPath(c, c, R, A0, A1)} fill="none" strokeWidth={sw} strokeLinecap="round" />
          {Math.abs(value - (bipolar ? 0.5 : 0)) > 0.001 &&
            <path className="k-arc" style={arcStyle} d={arc} fill="none" strokeWidth={sw} strokeLinecap="round" />}
          <circle className="k-body-out" cx={c} cy={c} r={bodyR + 2} />
          <circle className="k-body-in" cx={c} cy={c} r={bodyR} />
          <line className="k-point" x1={pix} y1={piy} x2={px - (px-c)*0.06} y2={py - (py-c)*0.06}
                strokeWidth={size === 'lg' ? 2.4 : 1.8} strokeLinecap="round" />
          <circle className="k-hub" cx={c} cy={c} r={size === 'lg' ? 3 : 2} />
          {modOn && <circle className="k-moddot" r={size === 'lg' ? 2.7 : 2}>
            <animateMotion dur="1.7s" repeatCount="indefinite" keyPoints="0;1;0" keyTimes="0;0.5;1"
                           calcMode="spline" keySplines="0.4 0 0.6 1;0.4 0 0.6 1" path={modPath} />
          </circle>}
        </svg>
      </div>
      {label && <div className="klabel">{label}</div>}
    </div>
  );
}

/* ---- Chip toggle (LED) ---- */
function Chip({ on, onClick, children, led = true }){
  return (
    <button className="chip" data-on={on ? '1' : '0'} onClick={onClick}>
      {led && <span className="led" />}{children}
    </button>
  );
}

/* ---- Select ---- */
function Selector({ value, options, onChange }){
  return (
    <div className="sel">
      <select value={value} onChange={(e) => onChange && onChange(e.target.value)}>
        {options.map((o) => <option key={o} value={o}>{o}</option>)}
      </select>
    </div>
  );
}

/* ---- small section header with optional icon + action ---- */
function PHead({ title, meta, icon, action }){
  return (
    <div className="phead">
      {icon && <span className="hicon">{icon}</span>}
      <h2>{title}</h2>
      <span className="hrule" />
      {meta && <span className="hmeta">{meta}</span>}
      {action}
    </div>
  );
}

/* ---- Power toggle (bypass) ---- */
function PowerBtn({ on, onClick, title = 'Bypass' }){
  return (
    <button className="pwrbtn" data-on={on ? '1' : '0'} onClick={onClick} title={title} aria-label={title}>
      <svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor"
           strokeWidth="2.2" strokeLinecap="round">
        <path d="M12 3 L12 11" />
        <path d="M6.5 6.5 a7.5 7.5 0 1 0 11 0" />
      </svg>
    </button>
  );
}

/* ---- Width / panorama dial (fan visualization) ---- */
function WidthDial({ value = 0.6, onChange, label = 'Width', format }){
  const [drag, setDrag] = useState(false);
  const stash = useRef({ y: 0, v: 0 });
  const D = 60, c = D / 2, Rf = c - 4;
  const half = 8 + value * 64;                 // half-angle of the stereo fan
  const [lx, ly] = polar(c, c, Rf, -half);
  const [rx, ry] = polar(c, c, Rf, half);
  const large = half * 2 > 180 ? 1 : 0;
  const fan = `M ${c} ${c} L ${lx.toFixed(2)} ${ly.toFixed(2)} A ${Rf} ${Rf} 0 ${large} 1 ${rx.toFixed(2)} ${ry.toFixed(2)} Z`;
  const onDown = useCallback((e) => {
    e.preventDefault();
    const pt = e.touches ? e.touches[0] : e;
    stash.current = { y: pt.clientY, v: value };
    setDrag(true);
    const move = (ev) => {
      const pp = ev.touches ? ev.touches[0] : ev;
      const dv = (stash.current.y - pp.clientY) / 240 * (ev.shiftKey ? 0.25 : 1);
      onChange && onChange(Math.max(0, Math.min(1, stash.current.v + dv)));
    };
    const up = () => { setDrag(false); window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', up); };
    window.addEventListener('pointermove', move); window.addEventListener('pointerup', up);
  }, [value, onChange]);
  const valTxt = format ? format(value) : Math.round(value * 200) + '%';
  return (
    <div className="knob lit">
      <div className={'dial' + (drag ? ' dragging' : '')} onPointerDown={onDown}
           onDoubleClick={() => onChange && onChange(0.6)} style={{ width: D, height: D }}>
        <div className="kval">{valTxt}</div>
        <svg width={D} height={D} viewBox={`0 0 ${D} ${D}`} style={{ overflow: 'visible' }}>
          <circle className="k-track" cx={c} cy={c} r={Rf} fill="none" strokeWidth="2.4" />
          <path d={fan} className="wd-fan" />
          <line x1={c} y1={c} x2={lx} y2={ly} className="wd-edge" strokeWidth="1.4" />
          <line x1={c} y1={c} x2={rx} y2={ry} className="wd-edge" strokeWidth="1.4" />
          <circle cx={lx} cy={ly} r="2.2" className="wd-dot" />
          <circle cx={rx} cy={ry} r="2.2" className="wd-dot" />
          <circle className="k-hub" cx={c} cy={c} r="2.4" />
        </svg>
      </div>
      {label && <div className="klabel">{label}</div>}
    </div>
  );
}


/* ---- Vertical Fader (for levels — the mixer metaphor) ---- */
function Fader({ value = 0.7, onChange, label, height = 100, format, lit = false }){
  const [drag, setDrag] = useState(false);
  const stash = useRef({ y: 0, v: 0 });
  const onDown = useCallback((e) => {
    e.preventDefault();
    const pt = e.touches ? e.touches[0] : e;
    stash.current = { y: pt.clientY, v: value };
    setDrag(true);
    const move = (ev) => {
      const p = ev.touches ? ev.touches[0] : ev;
      const fine = ev.shiftKey ? 0.3 : 1;
      const dv = (stash.current.y - p.clientY) / height * fine;
      onChange && onChange(Math.max(0, Math.min(1, stash.current.v + dv)));
    };
    const up = () => { setDrag(false); window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', up); };
    window.addEventListener('pointermove', move); window.addEventListener('pointerup', up);
  }, [value, onChange, height]);
  return (
    <div className={'fader' + (lit ? ' lit' : '')}>
      <div className={'fader-track' + (drag ? ' dragging' : '')} style={{ height }}
           onPointerDown={onDown} onDoubleClick={() => onChange && onChange(0.7)}>
        {format && <div className="fader-val">{format(value)}</div>}
        <div className="fader-fill" style={{ height: (value * 100) + '%' }} />
        <div className="fader-cap" style={{ bottom: `calc(${value * 100}% - 8px)` }} />
      </div>
      {label && <div className="klabel">{label}</div>}
    </div>
  );
}

Object.assign(window, { Knob, Fader, Chip, Selector, PHead, polar, arcPath, A0, A1, SWEEP });
