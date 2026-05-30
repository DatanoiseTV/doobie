/* ============================================================
   Doobie · App — wires every UI control to APVTS via JuceBridge
   ============================================================
   No useState() for parameter values; the JUCE WebSliderRelay /
   WebToggleButtonRelay / WebComboBoxRelay system is the single
   source of truth. Host automation, preset loads, and undo all
   flow through the same path as user knob-drags.
   ============================================================ */

const JB = window.JuceBridge;

/* ---- design key → APVTS id mapping ---------------------------------------
   Every entry is either a slider, toggle or choice. The matrix lets us keep
   the design's panel code in panels.jsx untouched -- they still talk in
   { p: {key: value}, setP: (key, value) } -- while the values + setters
   roundtrip through APVTS. */
const PARAM_MAP = {
  // --- input ---
  inTrim:     { kind: 'slider', id: 'inputDrive' },
  inLowCut:   { kind: 'slider', id: 'preHpFreq' },
  inHighCut:  { kind: 'slider', id: 'preLpFreq' },
  inBass:     { kind: 'slider', id: 'preBass'   },
  inTreble:   { kind: 'slider', id: 'preTreble' },
  // --- delay ---
  time:       { kind: 'slider', id: 'timeMs'   },
  feedback:   { kind: 'slider', id: 'feedback' },
  sync:       { kind: 'toggle', id: 'syncMode' },
  division:   { kind: 'choice', id: 'syncDiv',
                opts: ['1/64','1/32T','1/32','1/16T','1/16','1/8T','1/16.',
                       '1/8','1/4T','1/8.','1/4','1/2T','1/4.','1/2',
                       '1/1T','1/2.','1/1','2 bars','4 bars'] },
  character:  { kind: 'choice', id: 'delayMode', opts: ['Digital','Tape','BBD','Diffuse','Pitch'] },
  pingpong:   { kind: 'toggle', id: 'pingPong' },
  freeze:     { kind: 'toggle', id: 'freeze' },
  bypass:     { kind: 'toggle', id: 'delayBypass' },
  // --- tape character ---
  wow:        { kind: 'slider', id: 'wow' },
  flutter:    { kind: 'slider', id: 'flutter' },
  sat:        { kind: 'slider', id: 'drive' },
  age:        { kind: 'slider', id: 'hiss' },
  // --- feedback tone ---
  fbLowCut:   { kind: 'slider', id: 'hpFreq' },
  fbHighCut:  { kind: 'slider', id: 'lpFreq' },
  fbBass:     { kind: 'slider', id: 'bass'   },
  fbTreble:   { kind: 'slider', id: 'treble' },
  // --- reverb ---
  revType:    { kind: 'choice', id: 'reverbMode',
                opts: ['Off','Spring','Plate','Spring > Plate','Spring + Plate','Hall','Shimmer','Convolution','Gated'] },
  route:      { kind: 'choice', id: 'reverbRoute', opts: ['Post','Pre','In Feedback'], aliases: { post:'Post', pre:'Pre', fb:'In Feedback' } },
  revMix:     { kind: 'slider', id: 'reverbMix' },
  revSpring:  { kind: 'slider', id: 'springDecay' },
  revStone:   { kind: 'slider', id: 'springTone' },
  revDamp:    { kind: 'slider', id: 'plateDamp' },
  revMod:     { kind: 'slider', id: 'reverbMod' },
  revPlate:   { kind: 'slider', id: 'plateDecay' },
  revSize:    { kind: 'slider', id: 'plateSize' },
  revPre:     { kind: 'slider', id: 'platePredelay' },
  revWidth:   { kind: 'slider', id: 'width' },     // shares the global width knob
  // --- gated reverb sub-params (appear in the Reverb panel when revType=Gated) ---
  gateThr:    { kind: 'slider', id: 'gateThreshold' },
  gateHold:   { kind: 'slider', id: 'gateHold' },
  gateRel:    { kind: 'slider', id: 'gateRelease' },
  // --- output ---
  mix:        { kind: 'slider', id: 'mix' },
  width:      { kind: 'slider', id: 'width' },
  duck:       { kind: 'slider', id: 'duck' },
  output:     { kind: 'slider', id: 'outputGain' },
  // --- modulation sources ---
  lfo1Rate:   { kind: 'slider', id: 'lfo1Rate' },
  lfo1Depth:  { kind: 'slider', id: 'lfo1Depth' },
  lfo1Wave:   { kind: 'choice', id: 'lfo1Wave', opts: ['Sine','Triangle','Saw Up','Saw Down','Square','Random S&H'] },
  lfo2Rate:   { kind: 'slider', id: 'lfo2Rate' },
  lfo2Depth:  { kind: 'slider', id: 'lfo2Depth' },
  lfo2Wave:   { kind: 'choice', id: 'lfo2Wave', opts: ['Sine','Triangle','Saw Up','Saw Down','Square','Random S&H'] },
  envAtk:     { kind: 'slider', id: 'envAttack'  },
  envRel:     { kind: 'slider', id: 'envRelease' },
  envSens:    { kind: 'slider', id: 'envSens'    },
};

function bindParams(force) {
  const handles = {};
  Object.entries(PARAM_MAP).forEach(([uiKey, def]) => {
    let h;
    if (def.kind === 'slider') {
      h = window.Juce.getSliderState(def.id);
      h.valueChangedEvent.addListener(force);
    } else if (def.kind === 'toggle') {
      h = window.Juce.getToggleState(def.id);
      h.valueChangedEvent.addListener(force);
    } else {
      h = window.Juce.getComboBoxState(def.id);
      h.valueChangedEvent.addListener(force);
    }
    handles[uiKey] = { def, h };
  });
  return handles;
}

function makeP(handles) {
  // A plain object the panels can read with p.feedback etc. We rebuild
  // on every render so values are always live — re-renders are driven
  // by the listener on each relay.
  const p = {};
  Object.entries(handles).forEach(([uiKey, { def, h }]) => {
    if (def.kind === 'slider') p[uiKey] = h.getNormalisedValue();
    else if (def.kind === 'toggle') p[uiKey] = !!h.getValue();
    else { /* choice */
      const idx = h.getChoiceIndex();
      if (def.as === 'bool') {
        p[uiKey] = idx === 1;
      } else if (def.aliases) {
        const inv = Object.fromEntries(Object.entries(def.aliases).map(([a, b]) => [b, a]));
        p[uiKey] = inv[def.opts[idx]] || 'post';
      } else {
        p[uiKey] = def.opts[idx] || def.opts[0];
      }
    }
  });
  return p;
}

function setParam(handles, key, value) {
  const { def, h } = handles[key];
  if (def.kind === 'slider') {
    h.setNormalisedValue(Math.max(0, Math.min(1, value)));
  } else if (def.kind === 'toggle') {
    h.setValue(!!value);
  } else {
    let idx;
    if (def.as === 'bool') idx = value ? 1 : 0;
    else if (def.aliases) idx = def.opts.indexOf(def.aliases[value] || value);
    else idx = def.opts.indexOf(value);
    if (idx < 0) idx = 0;
    h.setChoiceIndex(idx);
  }
}

/* ---- per-head bindings (4 heads × 4 sub-params) -------------------------- */
function bindHeads(force) {
  const arr = [];
  for (let i = 0; i < 4; ++i) {
    const n = i + 1;
    const on   = window.Juce.getToggleState(`head${n}On`);
    const lvl  = window.Juce.getSliderState(`head${n}Level`);
    const pan  = window.Juce.getSliderState(`head${n}Pan`);
    const time = window.Juce.getSliderState(`head${n}Ratio`);
    on.valueChangedEvent.addListener(force);
    lvl.valueChangedEvent.addListener(force);
    pan.valueChangedEvent.addListener(force);
    time.valueChangedEvent.addListener(force);
    arr.push({ id: 'ABCD'[i], on, lvl, pan, time });
  }
  return arr;
}
function makeHeads(handles) {
  return handles.map(h => ({
    id: h.id,
    on: !!h.on.getValue(),
    level: h.lvl.getNormalisedValue(),
    pan:   h.pan.getNormalisedValue(),
    time:  h.time.getNormalisedValue(),
  }));
}
function setHead(handles, i, key, v) {
  const h = handles[i];
  if (key === 'on')    h.on.setValue(!!v);
  else if (key === 'level') h.lvl.setNormalisedValue(Math.max(0, Math.min(1, v)));
  else if (key === 'pan')   h.pan.setNormalisedValue(Math.max(0, Math.min(1, v)));
  else if (key === 'time')  h.time.setNormalisedValue(Math.max(0, Math.min(1, v)));
}

/* ---- mod matrix bindings ------------------------------------------------- */
const NUM_MOD_SLOTS = 8;
function bindMatrix(force) {
  const arr = [];
  for (let i = 0; i < NUM_MOD_SLOTS; ++i) {
    const n = i + 1;
    const src = window.Juce.getComboBoxState(`mod${n}Src`);
    const dst = window.Juce.getComboBoxState(`mod${n}Dst`);
    const amt = window.Juce.getSliderState(`mod${n}Amt`);
    src.valueChangedEvent.addListener(force);
    dst.valueChangedEvent.addListener(force);
    amt.valueChangedEvent.addListener(force);
    arr.push({ src, dst, amt });
  }
  return arr;
}
function makeMatrix(handles) {
  return handles.map(h => ({
    src: JB.MOD_SOURCES[h.src.getChoiceIndex()] || 'Off',
    dst: JB.MOD_DESTS[h.dst.getChoiceIndex()] || 'Off',
    amt: h.amt.getNormalisedValue() * 2 - 1,
  }));
}
function setMx(handles, i, key, v) {
  if (key === 'src') handles[i].src.setChoiceIndex(Math.max(0, JB.MOD_SOURCES.indexOf(v)));
  else if (key === 'dst') handles[i].dst.setChoiceIndex(Math.max(0, JB.MOD_DESTS.indexOf(v)));
  else if (key === 'amt') handles[i].amt.setNormalisedValue((Math.max(-1, Math.min(1, v)) + 1) * 0.5);
}

/* ---- App ---- */
function App() {
  const [, force] = useState(0);
  const bump = useCallback(() => force(n => n + 1), []);
  const handlesRef = useRef(null);
  const headsRef = useRef(null);
  const matrixRef = useRef(null);

  if (!handlesRef.current) handlesRef.current = bindParams(bump);
  if (!headsRef.current)   headsRef.current   = bindHeads(bump);
  if (!matrixRef.current)  matrixRef.current  = bindMatrix(bump);

  const p = makeP(handlesRef.current);
  const setP = useCallback((k, v) => setParam(handlesRef.current, k, v), []);
  const heads = makeHeads(headsRef.current);
  const sH = useCallback((i, k, v) => setHead(headsRef.current, i, k, v), []);
  const matrix = makeMatrix(matrixRef.current);
  const mX = useCallback((i, k, v) => setMx(matrixRef.current, i, k, v), []);

  // Compute the mod-destination indicator map from live matrix state.
  const mods = JB.useJuceModMap(NUM_MOD_SLOTS);

  // Native event subscriptions for live data
  const presetInfo = JB.useJuceEvent('presetInfo', { name: '—', cat: '' });
  const levels = JB.useJuceEvent('levels', { in: -90, delay: -90, reverb: -90, out: -90, peak: { in: -90, delay: -90, reverb: -90, out: -90, l: -90, r: -90 } });

  // Layout state (local UI only, not in APVTS)
  const [modOpen, setModOpen]   = useState(false);
  const [saveOpen, setSaveOpen] = useState(false);
  const rootRef = useRef(null);

  React.useEffect(() => {
    document.getElementById('root').classList.add('ready');
    const el = rootRef.current; if (!el) return;
    const fit = () => {
      const s = Math.min(window.innerWidth / 1520, window.innerHeight / 960);
      el.style.transform = `scale(${s})`;
    };
    fit();
    window.addEventListener('resize', fit);
    return () => window.removeEventListener('resize', fit);
  }, []);

  // Reel speed derived from delay TIME — same rule the native cassette used
  // (longer delay = slower capstan). Maps the 0..1 normalised time to a
  // sensible 0.4..2.4 spin multiplier so the reels visibly change with the
  // big TIME knob.
  const tapeSpeed = 2.4 - p.time * 2.0;

  // Driven from feedback proximity to self-oscillation (0.6+ = red).
  const fbCol = p.feedback > 0.5
    ? `color-mix(in oklab, var(--accent), var(--peak) ${Math.round((p.feedback - 0.5) / 0.5 * 100)}%)`
    : undefined;

  // Levels arrive as dBFS values; map to the design panels' 0..1 "base" they expect.
  const dbToBase = (db) => Math.max(0, Math.min(1, (db + 48) / 48));
  const stages = [
    { label: 'IN',     base: dbToBase(levels.in),     peakDb: levels.peak.in },
    { label: 'DELAY',  base: dbToBase(levels.delay),  peakDb: levels.peak.delay },
    { label: 'REVERB', base: dbToBase(levels.reverb), peakDb: levels.peak.reverb },
    { label: 'OUT',    base: dbToBase(levels.out),    peakDb: levels.peak.out },
  ];

  const presetClick = (dir) => window.Juce.backend.emitEvent(dir < 0 ? 'preset_prev' : 'preset_next', {});
  const onSave = () => setSaveOpen(true);

  return (
    <div id="stage">
      <div id="plugin" ref={rootRef} data-finish="glass" data-ticks="1">
        <Header preset={presetInfo} onPrev={() => presetClick(-1)} onNext={() => presetClick(1)}
                onSave={onSave} modOpen={modOpen} setModOpen={setModOpen} />
        <div className="body">
          <div className="flowbar">
            <VUStrip stages={stages} levels={levels} />
          </div>

          <div className="col-left">
            <InputPanel p={p} setP={setP} mods={mods} />
            <HeadsPanel heads={heads} setHead={sH} mods={mods} />
          </div>

          <div className="col-mid">
            <DelayPanel p={p} setP={setP} heads={heads} tapeSpeed={tapeSpeed} accent="var(--accent)" mods={mods} fbCol={fbCol} />
          </div>

          <div className="col-right">
            <FeedbackPanel p={p} setP={setP} mods={mods} />
            <ReverbPanel p={p} setP={setP} mods={mods} />
          </div>

          <div className="outbar">
            <OutputBar p={p} setP={setP} levels={levels} />
          </div>
        </div>
      </div>

      <ModDrawer open={modOpen} onClose={() => setModOpen(false)} p={p} setP={setP}
                 matrix={matrix} setMx={mX} numSlots={NUM_MOD_SLOTS} />

      <Modal open={saveOpen} title="Save preset" message="Pick a name for your patch."
             defaultValue={presetInfo.name || ''}
             onConfirm={(name) => {
               window.Juce.backend.emitEvent('preset_save', { name });
               setSaveOpen(false);
             }}
             onCancel={() => setSaveOpen(false)}
             confirmLabel="Save" />

      <KnobContextMenu />
    </div>
  );
}

ReactDOM.createRoot(document.getElementById('root')).render(<App />);
