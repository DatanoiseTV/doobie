/* ============================================================
   Doobie · panels & layout regions  (JUCE-bound)
   ============================================================
   Same shape as the design's panels.jsx -- panel functions
   accept (p, setP, ...) and render the controls. The single
   difference is that p comes from JUCE (live, host-driven)
   rather than React useState, and presets are routed through
   native events instead of an in-JS PRESETS array.
   ============================================================ */

const fmt = {
  pct:  (v) => Math.round(v * 100) + '%',
  db:   (v) => { const d = (v - 0.5) * 24; return (d > 0 ? '+' : '') + d.toFixed(1) + ' dB'; },
  trim: (v) => { const d = v * 36 - 18;    return (d > 0 ? '+' : '') + d.toFixed(1) + ' dB'; },
  ms:   (v) => Math.round(0.5 + v * 7999.5) + ' ms',
  pan:  (v) => { const d = Math.round((v - 0.5) * 200); return d === 0 ? 'C' : d < 0 ? 'L' + -d : 'R' + d; },
  sec:  (v) => (0.1 + v * 12).toFixed(1) + ' s',
  hz:   (min, max) => (v) => { const hz = min * Math.pow(max / min, v); return (hz >= 1000 ? (hz / 1000).toFixed(1) + 'k' : Math.round(hz)) + ' Hz'; },
  dbFs: (min, max) => (v) => (min + v * (max - min)).toFixed(1) + ' dB',
  msSkew: (min, max) => (v) => { const t = min * Math.pow(max / min, v); return t < 10 ? t.toFixed(1) + ' ms' : Math.round(t) + ' ms'; },
};

const Ico = {
  in:    <svg width="11" height="11" viewBox="0 0 11 11"><circle cx="5.5" cy="5.5" r="4.2" fill="none" stroke="currentColor" strokeWidth="1.4" /><circle cx="5.5" cy="5.5" r="1.4" fill="currentColor" /></svg>,
  heads: <svg width="11" height="11" viewBox="0 0 11 11"><rect x="0.6" y="1.4" width="2" height="8.2" rx="1" fill="currentColor" /><rect x="4.5" y="1.4" width="2" height="8.2" rx="1" fill="currentColor" /><rect x="8.4" y="1.4" width="2" height="8.2" rx="1" fill="currentColor" /></svg>,
  delay: <svg width="12" height="11" viewBox="0 0 12 11"><circle cx="3" cy="5.5" r="2.4" fill="currentColor" /><circle cx="9" cy="5.5" r="2.4" fill="none" stroke="currentColor" strokeWidth="1.3" /></svg>,
  fb:    <svg width="12" height="11" viewBox="0 0 12 11"><path d="M2 7 a4 4 0 1 1 8 0" fill="none" stroke="currentColor" strokeWidth="1.3" /><path d="M2 7 l-1 -2 2.4 .3" fill="none" stroke="currentColor" strokeWidth="1.3" /></svg>,
  rev:   <svg width="11" height="11" viewBox="0 0 11 11"><circle cx="5.5" cy="5.5" r="1.5" fill="currentColor" /><path d="M5.5 1.5 a4 4 0 0 1 0 8" fill="none" stroke="currentColor" strokeWidth="1.2" /><path d="M5.5 3.2 a2.3 2.3 0 0 1 0 4.6" fill="none" stroke="currentColor" strokeWidth="1.2" /></svg>,
  out:   <svg width="11" height="11" viewBox="0 0 11 11"><rect x="1" y="3.5" width="3" height="4" fill="currentColor" /><path d="M4 4 L9 1.5 V9.5 L4 7" fill="currentColor" /></svg>,
};

function KB({ label, k, p, setP, bipolar, format, lit, size = 'sm', mods, arcColor, modKey }) {
  return <Knob size={size} label={label} bipolar={bipolar} format={format} lit={lit}
               value={p[k]} onChange={(v) => setP(k, v)}
               mod={mods ? (mods[modKey || PARAM_MOD_KEY[k] || ''] || 0) : 0}
               arcColor={arcColor} />;
}

/* Map UI keys to the APVTS id used in the mod-dest hookup. Mirrors the
   bridge's DEST_TO_PARAMS reverse map -- both must stay in sync. */
const PARAM_MOD_KEY = {
  time: 'timeMs', feedback: 'feedback', mix: 'mix', width: 'width',
  revMix: 'reverbMix', fbHighCut: 'lpFreq', inHighCut: 'preLpFreq',
  sat: 'drive', wow: 'wow', flutter: 'flutter', age: 'hiss',
  duck: 'duck', revSize: 'plateSize', revPlate: 'plateDecay', revDamp: 'plateDamp',
  fbLowCut: 'hpFreq', inLowCut: 'preHpFreq',
  fbBass: 'bass', fbTreble: 'treble', inBass: 'preBass', inTreble: 'preTreble',
};

/* ============================== HEADER ============================== */
function Header({ preset, onPrev, onNext, onSave, modOpen, setModOpen }) {
  return (
    <div className="hdr">
      <div className="brand">
        <span className="logo">DO<b>O</b>BIE</span>
        <span className="tag">Analog Dub Delay</span>
      </div>
      <div className="spacer" />
      <span className="ver">{(window.DOOBIE_VERSION_STR || 'v0.13') + ' · main'}</span>
      <div className="preset">
        <button className="nav" onClick={onPrev} aria-label="Previous preset">‹</button>
        <button className="name" onClick={onNext}>
          <span className="t">{preset.name || '—'}</span>
          {preset.cat && <span className="cat">{preset.cat}</span>}
        </button>
        <button className="nav" onClick={onNext} aria-label="Next preset">›</button>
      </div>
      <button className="btn ghost" data-on={modOpen ? '1' : '0'} onClick={() => setModOpen(!modOpen)}>Mod</button>
      <button className="btn accent" onClick={onSave}>Save</button>
    </div>
  );
}

/* ============================== INPUT ============================== */
function InputPanel({ p, setP, mods }) {
  return (
    <div className="panel">
      <PHead title="Input" icon={Ico.in} meta="pre-delay tone" />
      <div className="row" style={{ gap: 18, alignItems: 'center' }}>
        <KB label="Gain" k="inTrim" p={p} setP={setP} format={fmt.trim} size="md" lit />
        <span className="divider" style={{ height: 56 }} />
        <div className="eqrow" style={{ flex: 1 }}>
          <KB label="Low Cut"  k="inLowCut"  p={p} setP={setP} format={fmt.hz(20, 800)}    mods={mods} />
          <KB label="High Cut" k="inHighCut" p={p} setP={setP} format={fmt.hz(1000, 20000)} mods={mods} />
          <KB label="Bass"     k="inBass"    p={p} setP={setP} bipolar format={fmt.db}     mods={mods} />
          <KB label="Treble"   k="inTreble"  p={p} setP={setP} bipolar format={fmt.db}     mods={mods} />
        </div>
      </div>
    </div>
  );
}

/* ============================== HEADS ============================== */
function HeadsPanel({ heads, setHead, mods }) {
  const GAP = 0.092;
  const clampTime = (i, v) => {
    let lo = 0, hi = 1;
    heads.forEach((h, j) => {
      if (j === i || !h.on) return;
      if (h.time <= heads[i].time) lo = Math.max(lo, h.time + GAP);
      else hi = Math.min(hi, h.time - GAP);
    });
    if (lo > hi) return heads[i].time;
    return Math.max(lo, Math.min(hi, v));
  };
  const freeSlot = (i) => {
    const taken = heads.filter((h, j) => j !== i && h.on).map(h => h.time).sort((a, b) => a - b);
    let v = heads[i].time;
    const clear = (x) => taken.every(t => Math.abs(t - x) >= GAP);
    if (clear(v)) return v;
    for (let d = GAP; d <= 1; d += GAP / 2) {
      if (v + d <= 1 && clear(v + d)) return v + d;
      if (v - d >= 0 && clear(v - d)) return v - d;
    }
    return v;
  };
  const toggle = (i, h) => {
    if (h.on) { setHead(i, 'on', false); }
    else { setHead(i, 'time', freeSlot(i)); setHead(i, 'on', true); }
  };
  // Build a per-head mod amount for the Pan + Time markers (the bridge
  // publishes Head1 Pan, Head2 Pan ... separately).
  const headPanMod = (i) => (mods && (mods['headPan' + i] || mods['pan' + i] || 0)) || 0;
  const headTimeMod = (i) => (mods && (mods['headRatio' + i] || 0)) || 0;
  return (
    <div className="panel" style={{ flex: 1 }}>
      <PHead title="Playback Heads" icon={Ico.heads} meta={heads.filter(h => h.on).length + '/4 on'} />
      <div className="hmix">
        {heads.map((h, i) =>
          <div key={h.id} className={'hstrip ' + (h.on ? 'on' : 'off')}>
            <button className="hbtn" onClick={() => toggle(i, h)} aria-label={'Head ' + h.id + (h.on ? ' on' : ' off')}>
              <span className="letter">{h.id}</span>
            </button>
            <Fader value={h.level} onChange={(v) => setHead(i, 'level', v)} height={102} format={fmt.pct} lit={h.on} />
            <Knob size="sm" label="Pan"  bipolar value={h.pan}  format={fmt.pan} mod={headPanMod(i)}
                  onChange={(v) => setHead(i, 'pan', v)} />
            <Knob size="sm" label="Time" value={h.time} format={fmt.pct} mod={headTimeMod(i)}
                  onChange={(v) => setHead(i, 'time', clampTime(i, v))} />
          </div>
        )}
      </div>
    </div>
  );
}

/* ============================== DELAY (hero) ============================== */
function DelayPanel({ p, setP, heads, tapeSpeed = 1, accent = 'var(--accent)', mods, fbCol }) {
  const tcChip = (key, label) => <Chip on={p[key]} onClick={() => setP(key, !p[key])}>{label}</Chip>;
  return (
    <div className="panel" style={{ flex: 1 }}>
      <PHead title="Delay" icon={Ico.delay}
             meta={p.bypass ? 'bypassed' : p.sync ? 'sync · ' + p.division : 'free'}
             action={<PowerBtn on={p.bypass} onClick={() => setP('bypass', !p.bypass)} title="Bypass delay" />} />
      <div className="tape-screen" style={{ padding: '6px 4px' }}>
        <div className="scan" />
        <TapeDeck heads={heads} playing={!p.bypass && !p.freeze} recording={false} speed={tapeSpeed} accent={accent} />
      </div>
      <div className="bigknobs" style={{ marginTop: 14 }}>
        <Knob size="lg" label="Time" value={p.time} lit mod={mods ? mods.timeMs || 0 : 0}
              format={p.sync ? () => p.division : fmt.ms}
              onChange={(v) => setP('time', v)} />
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10, flex: 1, maxWidth: 220, paddingTop: 6 }}>
          <div className="row" style={{ gap: 8 }}>
            <Chip on={p.sync} onClick={() => setP('sync', !p.sync)}>Sync</Chip>
            <div className="sel" style={{ flex: 1 }}>
              <select value={p.division} onChange={(e) => setP('division', e.target.value)} disabled={!p.sync}>
                {['1/64','1/32T','1/32','1/16T','1/16','1/8T','1/16.','1/8','1/4T','1/8.','1/4','1/2T','1/4.','1/2','1/1T','1/2.','1/1','2 bars','4 bars'].map(o => <option key={o} value={o}>{o}</option>)}
              </select>
            </div>
          </div>
          <div className="sel">
            <select value={p.character} onChange={(e) => setP('character', e.target.value)}>
              {['Digital','Tape','BBD','Diffuse','Pitch'].map(o => <option key={o} value={o}>{o}</option>)}
            </select>
          </div>
          <div className="cluster-label" style={{ marginTop: 2 }}>Character</div>
          <div className="row" style={{ gap: 8, flexWrap: 'wrap' }}>
            {tcChip('pingpong', 'Ping-Pong')}
            {tcChip('freeze',   'Freeze')}
          </div>
        </div>
        <Knob size="lg" label="Feedback" value={p.feedback} lit format={fmt.pct} arcColor={fbCol}
              mod={mods ? mods.feedback || 0 : 0}
              onChange={(v) => setP('feedback', v)} />
      </div>
      <div style={{ marginTop: 'auto', paddingTop: 14, borderTop: '1px solid var(--c-line-2)' }}>
        <div className="cluster-label" style={{ marginBottom: 10 }}>Tape Character</div>
        <div className="eqrow">
          <KB label="Wow"        k="wow"     p={p} setP={setP} format={fmt.pct} mods={mods} />
          <KB label="Flutter"    k="flutter" p={p} setP={setP} format={fmt.pct} mods={mods} />
          <KB label="Saturation" k="sat"     p={p} setP={setP} format={fmt.pct} mods={mods} />
          <KB label="Age"        k="age"     p={p} setP={setP} format={fmt.pct} mods={mods} />
        </div>
      </div>
    </div>
  );
}

/* ============================== FEEDBACK ============================== */
function FeedbackPanel({ p, setP, mods }) {
  return (
    <div className="panel">
      <PHead title="Feedback Loop" icon={Ico.fb} meta="in-loop tone" />
      <div className="eqrow">
        <KB label="Low Cut"  k="fbLowCut"  p={p} setP={setP} format={fmt.hz(20, 800)}     mods={mods} />
        <KB label="High Cut" k="fbHighCut" p={p} setP={setP} format={fmt.hz(1000, 20000)} mods={mods} />
        <KB label="Bass"     k="fbBass"    p={p} setP={setP} bipolar format={fmt.db}      mods={mods} />
        <KB label="Treble"   k="fbTreble"  p={p} setP={setP} bipolar format={fmt.db}      mods={mods} />
      </div>
    </div>
  );
}

/* ============================== REVERB ============================== */
function ReverbPanel({ p, setP, mods }) {
  const isGated = p.revType === 'Gated';
  const routeBtn = (k, lab) =>
    <button data-on={p.route === k ? '1' : '0'} onClick={() => setP('route', k)}>{lab}</button>;
  return (
    <div className="panel" style={{ flex: 1 }}>
      <PHead title="Reverb" icon={Ico.rev} meta={p.route === 'fb' ? 'in feedback' : p.route === 'pre' ? 'pre' : 'post'} />
      <div className="row" style={{ gap: 10, marginBottom: 10 }}>
        <div className="sel" style={{ flex: 1 }}>
          <select value={p.revType} onChange={(e) => setP('revType', e.target.value)}>
            {['Off','Spring','Plate','Spring > Plate','Spring + Plate','Hall','Shimmer','Convolution','Gated'].map(o => <option key={o} value={o}>{o}</option>)}
          </select>
        </div>
        <KB label="Mix" k="revMix" p={p} setP={setP} format={fmt.pct} size="md" lit mods={mods} />
      </div>
      <div className="rev-route" style={{ marginBottom: 14 }}>
        <span className="cluster-label">Route</span>
        <div className="seg">{routeBtn('pre', 'Pre')}{routeBtn('fb', 'In Feedback')}{routeBtn('post', 'Post')}</div>
      </div>
      <div className="eqrow" style={{ marginBottom: 14 }}>
        <KB label="Spring" k="revSpring" p={p} setP={setP} format={fmt.pct} mods={mods} />
        <KB label="S.Tone" k="revStone"  p={p} setP={setP} format={fmt.pct} mods={mods} />
        <KB label="Damp"   k="revDamp"   p={p} setP={setP} format={fmt.pct} mods={mods} />
        <KB label="Mod"    k="revMod"    p={p} setP={setP} format={fmt.pct} mods={mods} />
      </div>
      <div className="eqrow" style={{ marginBottom: 14 }}>
        <KB label="Decay"     k="revPlate" p={p} setP={setP} format={fmt.pct} mods={mods} />
        <KB label="Size"      k="revSize"  p={p} setP={setP} format={fmt.pct} mods={mods} />
        <KB label="Pre-Delay" k="revPre"   p={p} setP={setP} format={(v) => (v * 200).toFixed(0) + ' ms'} />
        <KB label="Width"     k="revWidth" p={p} setP={setP} format={fmt.pct} mods={mods} />
      </div>
      {isGated && (
        <div className="eqrow" style={{ marginBottom: 14, gridTemplateColumns: 'repeat(3, 1fr)' }}>
          <KB label="Gate Thr"  k="gateThr"  p={p} setP={setP} format={fmt.dbFs(-60, 0)} lit />
          <KB label="Gate Hold" k="gateHold" p={p} setP={setP} format={fmt.msSkew(1, 1000)} lit />
          <KB label="Gate Rel"  k="gateRel"  p={p} setP={setP} format={fmt.msSkew(0.5, 500)} lit />
        </div>
      )}
      <div style={{ marginTop: 'auto' }}>
        {/* The graph approximates RT60 from the decay knob (plate's actual
            decay parameter); size influences density / early reflections,
            not tail length, so feeding the curve off revSize gave the
            misleading impression the graph was decorative. */}
        <DecayGraph decay={p.revPlate} type={p.revType} height={104} />
      </div>
    </div>
  );
}

/* ============================== OUTPUT BAR ============================== */
function OutputBar({ p, setP, levels, mods }) {
  return (
    <div className="panel">
      <div className="outrow">
        <span className="hicon" style={{ color: 'var(--accent)' }}>{Ico.out}</span>
        <h2 style={{ margin: '0 18px 0 10px', fontSize: 11.5, fontWeight: 600, letterSpacing: '0.24em', textTransform: 'uppercase', color: 'var(--c-txt-2)' }}>Output</h2>
        <div className="knob-set">
          <KB label="Dry / Wet" k="mix"    p={p} setP={setP} format={fmt.pct}  size="md" lit mods={mods} />
          <WidthDial value={p.width} onChange={(v) => setP('width', v)} label="Width" format={fmt.pct} />
          <KB label="Duck"     k="duck"   p={p} setP={setP} format={fmt.pct}   size="md"     mods={mods} />
          <KB label="Output"   k="output" p={p} setP={setP} format={fmt.trim}  size="md" lit />
        </div>
        <span className="divider" style={{ height: 70 }} />
        <div className="out-meters">
          <DigitalMeter label="L" liveDb={levels.peak.l} big />
          <DigitalMeter label="R" liveDb={levels.peak.r} big scale />
        </div>
      </div>
    </div>
  );
}

/* ============================== METER BRIDGE (top) ============================== */
function VUStrip({ stages }) {
  return (
    <div className="bridge">
      {stages.map((s, i) =>
        <React.Fragment key={s.label}>
          {i > 0 && <span className="chev" aria-hidden="true">›</span>}
          <DigitalMeter label={s.label} liveDb={s.peakDb} />
        </React.Fragment>
      )}
    </div>
  );
}

/* ============================== MOD DRAWER ============================== */
function ModDrawer({ open, onClose, p, setP, matrix, setMx, numSlots }) {
  const [tab, setTab] = useState('sources');
  return (
    <>
      <div className={'drawer-scrim' + (open ? ' open' : '')} onClick={onClose} />
      <div className={'drawer' + (open ? ' open' : '')}>
        <div className="dh">
          <h2>Modulation</h2>
          <div className="tabs">
            <button data-on={tab === 'sources' ? '1' : '0'} onClick={() => setTab('sources')}>Sources</button>
            <button data-on={tab === 'matrix'  ? '1' : '0'} onClick={() => setTab('matrix')}>Matrix</button>
          </div>
          <button className="btn ghost dclose" onClick={onClose}>Close ✕</button>
        </div>
        <div className="dbody">
          {tab === 'sources' ?
            <div className="modgrid">
              <div className="modcard">
                <div className="subhead">LFO 1</div>
                <div className="row" style={{ gap: 18, alignItems: 'flex-start' }}>
                  <KB label="Rate"  k="lfo1Rate"  p={p} setP={setP} format={(v) => (0.001 + v * 20).toFixed(2) + ' Hz'} size="md" lit />
                  <KB label="Depth" k="lfo1Depth" p={p} setP={setP} format={fmt.pct} size="md" lit />
                  <div style={{ flex: 1 }}>
                    <div className="sel">
                      <select value={p.lfo1Wave} onChange={(e) => setP('lfo1Wave', e.target.value)}>
                        {['Sine','Triangle','Saw Up','Saw Down','Square','Random S&H'].map(o => <option key={o}>{o}</option>)}
                      </select>
                    </div>
                    <div style={{ height: 8 }} />
                    <WaveMini shape={p.lfo1Wave} rate={p.lfo1Rate} depth={p.lfo1Depth} />
                  </div>
                </div>
              </div>
              <div className="modcard">
                <div className="subhead">LFO 2</div>
                <div className="row" style={{ gap: 18, alignItems: 'flex-start' }}>
                  <KB label="Rate"  k="lfo2Rate"  p={p} setP={setP} format={(v) => (0.001 + v * 20).toFixed(2) + ' Hz'} size="md" lit />
                  <KB label="Depth" k="lfo2Depth" p={p} setP={setP} format={fmt.pct} size="md" lit />
                  <div style={{ flex: 1 }}>
                    <div className="sel">
                      <select value={p.lfo2Wave} onChange={(e) => setP('lfo2Wave', e.target.value)}>
                        {['Sine','Triangle','Saw Up','Saw Down','Square','Random S&H'].map(o => <option key={o}>{o}</option>)}
                      </select>
                    </div>
                    <div style={{ height: 8 }} />
                    <WaveMini shape={p.lfo2Wave} rate={p.lfo2Rate} depth={p.lfo2Depth} />
                  </div>
                </div>
              </div>
              <div className="modcard">
                <div className="subhead">Envelope Follower</div>
                <div className="row" style={{ gap: 18, justifyContent: 'space-around' }}>
                  <KB label="Attack"  k="envAtk"  p={p} setP={setP} format={fmt.msSkew(0.1, 500)} size="md" lit />
                  <KB label="Release" k="envRel"  p={p} setP={setP} format={fmt.msSkew(1, 2000)} size="md" lit />
                  <KB label="Sens"    k="envSens" p={p} setP={setP} format={fmt.pct}             size="md" lit />
                </div>
              </div>
            </div>
            :
            <div className="modcard">
              <div className="subhead" style={{ display: 'grid', gridTemplateColumns: '24px 1fr 22px 1fr 1.2fr 64px', gap: 10 }}>
                <span>#</span><span>Source</span><span /><span>Destination</span><span>Amount</span><span style={{ textAlign: 'right' }}>Value</span>
              </div>
              {matrix.map((m, i) =>
                <div className="mm-row" key={i}>
                  <span className="idx">{i + 1}</span>
                  <div className="sel">
                    <select value={m.src} onChange={(e) => setMx(i, 'src', e.target.value)}>
                      {JuceBridge.MOD_SOURCES.map(o => <option key={o}>{o}</option>)}
                    </select>
                  </div>
                  <span className="arr">→</span>
                  <div className="sel">
                    <select value={m.dst} onChange={(e) => setMx(i, 'dst', e.target.value)}>
                      {JuceBridge.MOD_DESTS.map(o => <option key={o}>{o}</option>)}
                    </select>
                  </div>
                  <input className="mslider" type="range" min="-1" max="1" step="0.001" value={m.amt}
                         onChange={(e) => setMx(i, 'amt', parseFloat(e.target.value))} />
                  <span className="mmval">{m.amt >= 0 ? '+' : ''}{m.amt.toFixed(3)}</span>
                </div>
              )}
            </div>
          }
        </div>
      </div>
    </>
  );
}

Object.assign(window, { Header, VUStrip, InputPanel, HeadsPanel, DelayPanel, FeedbackPanel, ReverbPanel, OutputBar, ModDrawer, fmt, KB, PARAM_MOD_KEY });
