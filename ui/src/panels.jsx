/* ============================================================
   Doobie · panels & layout regions  (JUCE-bound)
   ============================================================
   Same shape as the design's panels.jsx -- panel functions
   accept (p, setP, ...) and render the controls. The single
   difference is that p comes from JUCE (live, host-driven)
   rather than React useState, and presets are routed through
   native events instead of an in-JS PRESETS array.
   ============================================================ */

// ---------------------------------------------------------------------------
// Per-head sync-mode ratios. When the master time is locked to a tempo
// division, the per-head TIME knobs become a stepped picker over these
// musical fractions of the master delay instead of a continuous 0..1.
// Same idea as a Space-Echo head ladder, but adjustable per head.
// ---------------------------------------------------------------------------
const HEAD_DIV_RATIOS = [1.0, 0.75, 2 / 3, 0.5, 3 / 8, 1 / 3, 0.25, 1 / 6, 0.125];
const HEAD_DIV_NAMES  = ['1/1', '3/4', '2/3', '1/2', '3/8', '1/3', '1/4', '1/6', '1/8'];
function snapHeadDiv (v) {
  let best = 0, bestD = Infinity;
  for (let i = 0; i < HEAD_DIV_RATIOS.length; ++i) {
    const d = Math.abs (HEAD_DIV_RATIOS[i] - v);
    if (d < bestD) { bestD = d; best = i; }
  }
  return { ratio: HEAD_DIV_RATIOS[best], name: HEAD_DIV_NAMES[best], idx: best };
}

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
  duck: 'duck',
  revSpring: 'springDecay', revStone: 'springTone', revMod: 'reverbMod',
  revPlate: 'plateDecay', revSize: 'plateSize', revDamp: 'plateDamp',
  revPre: 'platePredelay',
  fbLowCut: 'hpFreq', inLowCut: 'preHpFreq',
  fbBass: 'bass', fbTreble: 'treble', inBass: 'preBass', inTreble: 'preTreble',
  // Hardware-port additions (v0.14): mods that destination-target these
  // need the JS bridge to know which APVTS param to light.
  inFilterCutoff: 'inFilterCutoff', inFilterRes: 'inFilterRes',
  phaserRate: 'phaserRate', phaserDepth: 'phaserDepth', phaserMix: 'phaserMix',
  output: 'outputGain',
};

/* ============================== HEADER ============================== */
function Header({ preset, onPrev, onNext, onSave, modOpen, setModOpen, onBrowse }) {
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
        <button className="name" onClick={() => onBrowse && onBrowse()} title="Browse presets">
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
  // Input multimode filter (ported from hardware). Its subsection collapses
  // to a single TYPE chip + on-LED when off so the EQ knobs stay the
  // headline of the panel; cutoff + res appear only when the filter is on,
  // matching the hardware's "OFF = true bypass" framing.
  const fOn = !!p.inFilterOn;
  const typeSeg = (k, lab) =>
    <button data-on={p.inFilterType === k ? '1' : '0'}
            onClick={() => setP('inFilterType', k)}>{lab}</button>;
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
      {/* ----- Input multimode filter (TPT-SVF, ported from the hardware
              Keinedelay/DFM build). OFF = true bypass. ----- */}
      <div className="route-row" style={{ marginTop: 14, marginBottom: fOn ? 12 : 0 }}>
        <Chip on={fOn} onClick={() => setP('inFilterOn', !fOn)}>Filter</Chip>
        {fOn && (
          <div className="seg">{typeSeg('LP', 'LP')}{typeSeg('HP', 'HP')}{typeSeg('BP', 'BP')}</div>
        )}
      </div>
      {fOn && (
        <div className="eqrow" style={{ gridTemplateColumns: 'repeat(2, 1fr)' }}>
          <KB label="Cutoff" k="inFilterCutoff" p={p} setP={setP}
              format={(v) => v < 1 ? Math.round(v * 18000) + ' Hz' : (v).toFixed(0) + ' Hz'}
              mods={mods} modKey="inFilterCutoff" lit />
          <KB label="Reso"   k="inFilterRes"    p={p} setP={setP}
              format={fmt.pct} mods={mods} modKey="inFilterRes" />
        </div>
      )}
    </div>
  );
}

/* ============================== HEADS ============================== */
function HeadsPanel({ heads, setHead, mods, synced }) {
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
            <Knob size="sm" label="Time" value={h.time}
                  format={synced ? (v) => snapHeadDiv (v).name : fmt.pct}
                  mod={headTimeMod(i)}
                  onChange={(v) => {
                    // Sync mode: snap to musical fractions of the master delay
                    // (Space-Echo head ladder feel). Anti-collision still
                    // applies so two heads can't share a slot.
                    const target = synced ? snapHeadDiv (v).ratio : v;
                    setHead(i, 'time', clampTime (i, target));
                  }} />
            {/* Per-head additive offset in ms (signed, ±200 ms). Stacks on
                top of the ratio-driven tap so micro-timing / haas-style
                widening between heads is independent of the master delay.
                Bipolar knob, ms display, separate from the wow/flutter-
                modulated head TIME above. */}
            <Knob size="sm" label="Offset" value={h.offset} bipolar
                  format={(v) => { const d = Math.round ((v - 0.5) * 400); return (d > 0 ? '+' : '') + d + ' ms'; }}
                  onChange={(v) => setHead(i, 'offset', v)} />
          </div>
        )}
      </div>
    </div>
  );
}

// MIDI note number -> "C4", "F#3", etc. Returns "—" for -1 (no note yet).
// C3 == 60 is the unison reference for the MIDI pitch mode.
const MIDI_NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
function midiNoteName (n) {
  if (n == null || n < 0) return '—';
  const oct = Math.floor (n / 12) - 1;
  return MIDI_NOTE_NAMES[n % 12] + oct;
}

/* ============================== DELAY (hero) ============================== */
function DelayPanel({ p, setP, heads, tapeSpeed = 1, accent = 'var(--accent)', mods, fbCol, midiNote }) {
  const tcChip = (key, label) => <Chip on={p[key]} onClick={() => setP(key, !p[key])}>{label}</Chip>;
  // Pitch-character interval picker. Normalised 0..1 from the slider relay
  // maps to -24..+24 st in unit steps (49 discrete steps). Sliding lands on
  // the named musical interval; the chip row jumps to common ones with
  // one click. Same shape as the shimmer reverb picker so both feel like
  // siblings of the same control.
  const pSemis    = Math.round (p.pitchSemis * 48) - 24;
  const setPSemis = (s) => setP ('pitchSemis', (s + 24) / 48);
  const pSemiChips = [-24, -19, -12, -7, -5, 0, 5, 7, 12, 19, 24];
  const pSemiName  = (s) => {
    const sign = s > 0 ? '+' : s < 0 ? '−' : '';
    const abs  = Math.abs (s);
    const tag  = abs === 0  ? 'unison'
              : abs === 5  ? '4th'
              : abs === 7  ? '5th'
              : abs === 12 ? 'octave'
              : abs === 19 ? '8va+5'
              : abs === 24 ? '2 octaves'
              : '';
    return sign + abs + ' st' + (tag ? ' · ' + tag : '');
  };
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
          {p.character === 'Pitch' && (
            <div className="shimmer-int" style={{ marginBottom: 4 }}>
              <div className="shimmer-int-hd">
                <button className={'midi-btn' + (p.pitchOn ? ' on' : '')}
                        onClick={() => setP ('pitchOn', !p.pitchOn)}
                        title="Bypass the pitch shifter (saturation still runs; clears the FFT latency)">
                  {p.pitchOn ? 'ON' : 'OFF'}
                </button>
                <span className="cluster-label" style={{ marginLeft: 4 }}>Interval</span>
                <span className="shimmer-int-val">
                  {pSemiName (pSemis)}
                  {p.midiPitchMode && <> · MIDI <span className="midi-note">{midiNoteName (midiNote)}</span></>}
                </span>
                <span style={{ flex: 1 }} />
                {p.midiPitchMode && (
                  <button className={'midi-btn' + (p.midiPortaOn ? ' on' : '')}
                          onClick={() => setP ('midiPortaOn', !p.midiPortaOn)}
                          title="Glide between notes over the porta time (pitch bend always rides on top, ±2 st)">
                    PORTA
                  </button>
                )}
                <button className={'midi-btn' + (p.midiPitchMode ? ' on' : '')}
                        onClick={() => setP ('midiPitchMode', !p.midiPitchMode)}
                        title="Drive the interval from incoming MIDI notes (C3 = unison, pitch bend = ±2 st)">
                  MIDI
                </button>
              </div>
              {p.midiPitchMode && p.midiPortaOn && (
                <div className="porta-row">
                  <span className="porta-lab">Glide</span>
                  <input type="range" min="0" max="1" step="0.001" value={p.midiPortaMs}
                         onChange={(e) => setP ('midiPortaMs', parseFloat (e.target.value))} />
                  <span className="porta-val">{Math.round (p.midiPortaMs * 2000)} ms</span>
                </div>
              )}
              <div className="shimmer-int-chips" style={p.midiPitchMode ? { opacity: 0.45, pointerEvents: 'none' } : null}>
                {pSemiChips.map (s =>
                  <button key={s} data-on={pSemis === s ? '1' : '0'} onClick={() => setPSemis (s)}>
                    {s > 0 ? '+' + s : s}
                  </button>
                )}
              </div>
            </div>
          )}
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
    <div className="panel compact">
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

/* ============================== PHASER (hardware port) ============================== */
// Six-stage all-pass with feedback. Three routing modes match the reverb's
// insert points so signal flow stays one mental model: PRE = into the delay
// input, IN FEEDBACK = cumulative per repeat (flange-y at high feedback),
// POST = on the wet echoes (sits BEFORE the reverb's own insert).
function PhaserPanel({ p, setP, mods }) {
  const on = !!p.phaserOn;
  const routeBtn = (k, lab) =>
    <button data-on={p.phaserRoute === k ? '1' : '0'}
            onClick={() => setP('phaserRoute', k)}>{lab}</button>;
  return (
    <div className="panel compact">
      <PHead title="Phaser" icon={Ico.fb}
             meta={on ? (p.phaserRoute === 'fb' ? 'in feedback' : p.phaserRoute === 'pre' ? 'pre' : 'post') : 'off'}
             action={<PowerBtn on={on} onClick={() => setP('phaserOn', !on)} title="Bypass phaser" />} />
      <div className="route-row" style={{ opacity: on ? 1 : 0.45 }}>
        <span className="route-lab">Route</span>
        <div className="seg">{routeBtn('pre', 'Pre')}{routeBtn('fb', 'In Feedback')}{routeBtn('post', 'Post')}</div>
      </div>
      <div className="eqrow" style={{ gridTemplateColumns: 'repeat(4, 1fr)', opacity: on ? 1 : 0.45 }}>
        <KB label="Rate"  k="phaserRate"  p={p} setP={setP}
            format={(v) => (0.01 + v * 7.99).toFixed(2) + ' Hz'}
            mods={mods} modKey="phaserRate" lit />
        <KB label="Depth" k="phaserDepth" p={p} setP={setP}
            format={fmt.pct} mods={mods} modKey="phaserDepth" />
        <KB label="Fb"    k="phaserFb"    p={p} setP={setP}
            format={fmt.pct} />
        <KB label="Mix"   k="phaserMix"   p={p} setP={setP}
            format={fmt.pct} mods={mods} modKey="phaserMix" lit />
      </div>
    </div>
  );
}

/* ============================== REVERB ============================== */
function ReverbPanel({ p, setP, mods, irInfo, midiNote }) {
  const isGated   = p.revType === 'Gated';
  const isConv    = p.revType === 'Convolution';
  const isShimmer = p.revType === 'Shimmer';
  // Shimmer interval picker. The underlying APVTS param is integer
  // semitones over -24..+24 (49 discrete steps), so the normalised 0..1 we
  // get from the slider maps as v -> round(v * 48) - 24. Sliding lands on
  // the named musical interval; the chips below jump to common ones in
  // one click.
  const semis     = Math.round (p.shimmerSemis * 48) - 24;
  const setSemis  = (s) => setP ('shimmerSemis', (s + 24) / 48);
  const semiName  = (s) => {
    const sign = s > 0 ? '+' : s < 0 ? '−' : '';
    const abs  = Math.abs (s);
    const tag  = abs === 0  ? 'unison'
              : abs === 5  ? '4th'
              : abs === 7  ? '5th'
              : abs === 12 ? 'octave'
              : abs === 19 ? '8va+5'
              : abs === 24 ? '2 octaves'
              : '';
    return sign + abs + ' st' + (tag ? ' · ' + tag : '');
  };
  const semiChips = [-24, -19, -12, -7, -5, 0, 5, 7, 12, 19, 24];
  const routeBtn = (k, lab) =>
    <button data-on={p.route === k ? '1' : '0'} onClick={() => setP('route', k)}>{lab}</button>;
  return (
    <div className="panel compact" style={{ flex: 1 }}>
      <PHead title="Reverb" icon={Ico.rev} meta={p.route === 'fb' ? 'in feedback' : p.route === 'pre' ? 'pre' : 'post'} />
      <div className="row" style={{ gap: 10, marginBottom: 8 }}>
        <div className="sel" style={{ flex: 1 }}>
          <select value={p.revType} onChange={(e) => setP('revType', e.target.value)}>
            {['Off','Spring','Plate','Spring > Plate','Spring + Plate','Hall','Shimmer','Convolution','Gated'].map(o => <option key={o} value={o}>{o}</option>)}
          </select>
        </div>
        <KB label="Mix" k="revMix" p={p} setP={setP} format={fmt.pct} size="md" lit mods={mods} />
      </div>
      <div className="route-row">
        <span className="route-lab">Route</span>
        <div className="seg">{routeBtn('pre', 'Pre')}{routeBtn('fb', 'In Feedback')}{routeBtn('post', 'Post')}</div>
      </div>
      {isConv && <IRPicker irInfo={irInfo} />}
      {/* Convolution doesn't use the spring/plate engine — the IR is the
          reverb. Only IR gain + width apply, so the 8-knob block collapses
          to two knobs and the panel fits comfortably alongside the Phaser. */}
      {isConv ? (
        <div className="eqrow" style={{ marginBottom: 8, gridTemplateColumns: 'repeat(2, 1fr)' }}>
          <KB label="IR Gain" k="irGain"   p={p} setP={setP} format={fmt.db}  mods={mods} lit />
          <KB label="Width"   k="revWidth" p={p} setP={setP} format={fmt.pct} mods={mods} />
        </div>
      ) : (
        <>
          <div className="eqrow" style={{ marginBottom: 8 }}>
            <KB label="Spring" k="revSpring" p={p} setP={setP} format={fmt.pct} mods={mods} />
            <KB label="S.Tone" k="revStone"  p={p} setP={setP} format={fmt.pct} mods={mods} />
            <KB label="Damp"   k="revDamp"   p={p} setP={setP} format={fmt.pct} mods={mods} />
            <KB label="Mod"    k="revMod"    p={p} setP={setP} format={fmt.pct} mods={mods} />
          </div>
          <div className="eqrow" style={{ marginBottom: 8 }}>
            <KB label="Decay"     k="revPlate" p={p} setP={setP} format={fmt.pct} mods={mods} />
            <KB label="Size"      k="revSize"  p={p} setP={setP} format={fmt.pct} mods={mods} />
            <KB label="Pre-Delay" k="revPre"   p={p} setP={setP} format={(v) => (v * 200).toFixed(0) + ' ms'} />
            <KB label="Width"     k="revWidth" p={p} setP={setP} format={fmt.pct} mods={mods} />
          </div>
        </>
      )}
      {isGated && (
        <div className="eqrow" style={{ marginBottom: 8, gridTemplateColumns: 'repeat(3, 1fr)' }}>
          <KB label="Gate Thr"  k="gateThr"  p={p} setP={setP} format={fmt.dbFs(-60, 0)} lit />
          <KB label="Gate Hold" k="gateHold" p={p} setP={setP} format={fmt.msSkew(1, 1000)} lit />
          <KB label="Gate Rel"  k="gateRel"  p={p} setP={setP} format={fmt.msSkew(0.5, 500)} lit />
        </div>
      )}
      {isShimmer && (
        <div className="shimmer-int">
          <div className="shimmer-int-hd">
            <span className="cluster-label">Interval</span>
            <span className="shimmer-int-val">
              {semiName (semis)}
              {p.midiPitchMode && <> · MIDI <span className="midi-note">{midiNoteName (midiNote)}</span></>}
            </span>
            <span style={{ flex: 1 }} />
            {p.midiPitchMode && (
              <button className={'midi-btn' + (p.midiPortaOn ? ' on' : '')}
                      onClick={() => setP ('midiPortaOn', !p.midiPortaOn)}
                      title="Glide between notes over the porta time (pitch bend always rides on top, ±2 st)">
                PORTA
              </button>
            )}
            <button className={'midi-btn' + (p.midiPitchMode ? ' on' : '')}
                    onClick={() => setP ('midiPitchMode', !p.midiPitchMode)}
                    title="Drive the interval from incoming MIDI notes (C3 = unison, pitch bend = ±2 st)">
              MIDI
            </button>
          </div>
          {p.midiPitchMode && p.midiPortaOn && (
            <div className="porta-row">
              <span className="porta-lab">Glide</span>
              <input type="range" min="0" max="1" step="0.001" value={p.midiPortaMs}
                     onChange={(e) => setP ('midiPortaMs', parseFloat (e.target.value))} />
              <span className="porta-val">{Math.round (p.midiPortaMs * 2000)} ms</span>
            </div>
          )}
          <div className="shimmer-int-chips" style={p.midiPitchMode ? { opacity: 0.45, pointerEvents: 'none' } : null}>
            {semiChips.map (s =>
              <button key={s} data-on={semis === s ? '1' : '0'} onClick={() => setSemis (s)}>
                {s > 0 ? '+' + s : s}
              </button>
            )}
          </div>
        </div>
      )}
      <div style={{ marginTop: 'auto' }}>
        {/* RT60 approximation: decay-knob driven; size affects density not
            tail. Shorter than the v0.13 layout so the panel still fits
            below the Phaser. */}
        <DecayGraph decay={p.revPlate} type={p.revType} height={64} />
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
          {/* Auto-gain pill — slow program leveler + fast ceiling catch on
              the output. Tames feedback near self-oscillation without
              crushing dynamics. ON by default; LED-style chip so it reads
              at a glance during live use. */}
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4, marginLeft: 4 }}>
            <Chip on={p.autoGain} onClick={() => setP('autoGain', !p.autoGain)}>Auto Gain</Chip>
            <span style={{ fontSize: 9, letterSpacing: '0.16em', textTransform: 'uppercase', color: 'var(--c-txt-3)' }}>
              {p.autoGain ? 'leveler' : 'bypass'}
            </span>
          </div>
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

Object.assign(window, { Header, VUStrip, InputPanel, HeadsPanel, DelayPanel, FeedbackPanel, PhaserPanel, ReverbPanel, OutputBar, ModDrawer, fmt, KB, PARAM_MOD_KEY });
