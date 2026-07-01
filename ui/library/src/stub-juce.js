/* ============================================================
   Doobie · stub JUCE bridge (standalone / design-tool)
   ============================================================
   Reimplements the exact surface the real WebEditor injects
   (window.Juce + native events + getNativeFunction) with an
   in-memory, listener-driven model seeded with musically
   sensible demo values. The unmodified ui/src components run
   against this identically to how they run in the plugin —
   only the audio engine is absent. Everything stays live and
   interactive: dragging a knob updates state and re-renders,
   exactly as it does over the APVTS relays.
   ============================================================ */
(function (global) {
  // --- small listenable, mirrors JUCE's valueChangedEvent.addListener ---
  function makeEvent() {
    const ls = [];
    return {
      addListener: (cb) => { ls.push(cb); return cb; },
      removeListener: (cb) => { const i = ls.indexOf(cb); if (i >= 0) ls.splice(i, 1); },
      _fire: () => ls.slice().forEach((cb) => { try { cb(); } catch (_) {} }),
    };
  }

  // --- seeded defaults so the demo UI looks alive, not a wall of zeros ---
  // Sliders are normalised 0..1; anything not listed falls back to 0.5.
  const SLIDER_DEFAULTS = {
    inputDrive: 0.55, preHpFreq: 0.12, preLpFreq: 0.86, preBass: 0.55, preTreble: 0.48,
    inFilterCutoff: 0.6, inFilterRes: 0.3,
    phaserRate: 0.35, phaserDepth: 0.5, phaserFb: 0.4, phaserMix: 0.3,
    timeMs: 0.42, feedback: 0.46,
    wow: 0.28, flutter: 0.22, drive: 0.4, hiss: 0.18,
    hpFreq: 0.14, lpFreq: 0.82, bass: 0.52, treble: 0.5, hpRes: 0.25, lpRes: 0.25,
    reverbMix: 0.32, springDecay: 0.5, springTone: 0.5, plateDamp: 0.45, reverbMod: 0.3,
    plateDecay: 0.55, plateSize: 0.6, platePredelay: 0.2, width: 0.62,
    gateThreshold: 0.4, gateHold: 0.35, gateRelease: 0.4,
    shimmerSemis: 0.5, pitchSemis: 0.5, pitchSpread: 0.3,
    revHpFreq: 0.1, revLpFreq: 0.85,
    midiPortaMs: 0.3, irGain: 0.5, irSpeed: 0.5,
    mix: 0.36, duck: 0.3, outputGain: 0.5,
    duckCrossLow: 0.3, duckCrossHigh: 0.7,
    lfo1Rate: 0.35, lfo1Depth: 0.5, lfo1Smooth: 0.2, lfo1Offset: 0.5,
    lfo2Rate: 0.5, lfo2Depth: 0.35, lfo2Smooth: 0.2, lfo2Offset: 0.5,
    lfo3Rate: 0.28, lfo3Depth: 0.4, lfo3Smooth: 0.2, lfo3Offset: 0.5,
    lfo4Rate: 0.6, lfo4Depth: 0.3, lfo4Smooth: 0.2, lfo4Offset: 0.5,
    envAttack: 0.3, envRelease: 0.45, envSens: 0.55,
    envFilterCutoff: 0.6, envFilterRes: 0.3,
    // per-head level/pan/ratio/offset
    head1Level: 0.8, head1Pan: 0.35, head1Ratio: 0.25, head1Offset: 0.5,
    head2Level: 0.6, head2Pan: 0.65, head2Ratio: 0.5, head2Offset: 0.5,
    head3Level: 0.45, head3Pan: 0.45, head3Ratio: 0.62, head3Offset: 0.5,
    head4Level: 0.3, head4Pan: 0.6, head4Ratio: 0.78, head4Offset: 0.5,
    // mod matrix amounts (0.5 == 0 bipolar)
    mod1Amt: 0.72, mod2Amt: 0.6, mod3Amt: 0.5, mod4Amt: 0.5,
    mod5Amt: 0.5, mod6Amt: 0.5, mod7Amt: 0.5, mod8Amt: 0.5,
  };
  const TOGGLE_DEFAULTS = {
    inFilterOn: true, phaserOn: false, syncMode: false, pingPong: true,
    freeze: false, delayBypass: false, feedbackKill: false,
    pitchOn: false, midiPitchMode: false, midiPortaOn: false,
    outLevelerOn: true, duckMultiband: false,
    envFilterOn: false,
    lfo1Sync: false, lfo2Sync: false, lfo3Sync: false, lfo4Sync: false,
    head1On: true, head2On: true, head3On: true, head4On: false,
  };
  // Choice params store a 0-based index. Seed indices that give a rich look.
  const CHOICE_DEFAULTS = {
    inFilterType: 0, phaserRoute: 0, syncDiv: 7, delayMode: 1 /* Tape */,
    reverbMode: 3 /* Spring > Plate */, reverbRoute: 0,
    pitchAlgo: 0, pitchRoute: 0,
    lfo1Wave: 0, lfo2Wave: 1, lfo3Wave: 4, lfo4Wave: 2,
    lfo1Div: 7, lfo2Div: 7, lfo3Div: 7, lfo4Div: 7,
    envFilterType: 0,
    // one live mod slot for flavour: LFO 1 -> Delay Time
    mod1Src: 1, mod1Dst: 1, mod1Mode: 0,
    mod2Src: 2, mod2Dst: 2, mod2Mode: 0,
    mod3Src: 0, mod3Dst: 0, mod3Mode: 0,
    mod4Src: 0, mod4Dst: 0, mod4Mode: 0,
    mod5Src: 0, mod5Dst: 0, mod5Mode: 0,
    mod6Src: 0, mod6Dst: 0, mod6Mode: 0,
    mod7Src: 0, mod7Dst: 0, mod7Mode: 0,
    mod8Src: 0, mod8Dst: 0, mod8Mode: 0,
  };

  const sliderStates = {}, toggleStates = {}, comboStates = {};

  function getSliderState(id) {
    if (!sliderStates[id]) {
      const ev = makeEvent();
      let v = id in SLIDER_DEFAULTS ? SLIDER_DEFAULTS[id] : 0.5;
      sliderStates[id] = {
        getNormalisedValue: () => v,
        setNormalisedValue: (nv) => { v = Math.max(0, Math.min(1, nv)); ev._fire(); },
        valueChangedEvent: ev,
      };
    }
    return sliderStates[id];
  }
  function getToggleState(id) {
    if (!toggleStates[id]) {
      const ev = makeEvent();
      let v = id in TOGGLE_DEFAULTS ? TOGGLE_DEFAULTS[id] : false;
      toggleStates[id] = {
        getValue: () => v,
        setValue: (b) => { v = !!b; ev._fire(); },
        valueChangedEvent: ev,
      };
    }
    return toggleStates[id];
  }
  function getComboBoxState(id) {
    if (!comboStates[id]) {
      const ev = makeEvent();
      let idx = id in CHOICE_DEFAULTS ? CHOICE_DEFAULTS[id] : 0;
      comboStates[id] = {
        getChoiceIndex: () => idx,
        setChoiceIndex: (i) => { idx = Math.max(0, i | 0); ev._fire(); },
        valueChangedEvent: ev,
      };
    }
    return comboStates[id];
  }

  // --- native events (levels / presetInfo / irInfo), 30 Hz in the plugin ---
  const listeners = {};
  function addEventListener(name, cb) { (listeners[name] || (listeners[name] = [])).push(cb); }
  function removeEventListener(name, cb) {
    const a = listeners[name]; if (!a) return; const i = a.indexOf(cb); if (i >= 0) a.splice(i, 1);
  }
  function emit(name, payload) { (listeners[name] || []).slice().forEach((cb) => { try { cb(payload); } catch (_) {} }); }
  // emitEvent is the UI -> native channel (preset load/save, IR ops). No-op
  // here beyond keeping the demo consistent (e.g. preset name echo).
  function emitEvent(name, data) {
    if (name === 'preset_save' && data && data.name) queueMicrotask(() => emit('presetInfo', { name: data.name, cat: 'USER', dirty: false }));
    if (name === 'preset_load' && data && data.name) queueMicrotask(() => emit('presetInfo', { name: data.name, cat: 'DUB', dirty: false }));
  }

  // Seeded static payloads. levels can be gently animated for lively meters
  // via startLevelAnimation(); default is a nonzero static frame so previews
  // are deterministic.
  const staticLevels = {
    in: -14, delay: -18, reverb: -22, out: -12, midiNote: -1, grDb: 0, env: 0.35,
    lfo1v: 0.4, lfo2v: -0.2, lfo3v: 0.1, lfo4v: 0.3,
    headMag: [0.72, 0.5, 0.34, 0.0],
    peak: { in: -8, delay: -12, reverb: -16, out: -6, l: -7, r: -9 },
  };
  const presetInfo = { name: 'Dub Chamber', cat: 'DUB', dirty: false };
  const irInfo = { hasIR: true, factoryIndex: 0, isFactory: true, isFile: false, name: 'Concert Hall' };

  function replayInitial(name, cb) {
    if (name === 'levels') cb(staticLevels);
    else if (name === 'presetInfo') cb(presetInfo);
    else if (name === 'irInfo') cb(irInfo);
  }

  const backend = {
    addEventListener: (name, cb) => { addEventListener(name, cb); queueMicrotask(() => replayInitial(name, cb)); },
    removeEventListener,
    emitEvent,
  };

  // Optional: animate levels + LFO/env values so meters and mod dots move.
  let animRAF = 0;
  function startLevelAnimation() {
    if (animRAF) return;
    let t0 = null;
    const step = (ts) => {
      if (t0 == null) t0 = ts;
      const t = (ts - t0) / 1000;
      const s = (f, ph = 0) => Math.sin(2 * Math.PI * f * t + ph);
      const lvl = {
        in: -14 + 6 * Math.abs(s(1.7)), delay: -18 + 8 * Math.abs(s(0.9, 1)),
        reverb: -22 + 6 * Math.abs(s(0.5, 2)), out: -12 + 5 * Math.abs(s(1.3, 0.5)),
        midiNote: -1, grDb: 2 * Math.abs(s(0.7)), env: 0.3 + 0.25 * (0.5 + 0.5 * s(0.8)),
        lfo1v: s(0.35), lfo2v: s(0.5, 1), lfo3v: s(0.28, 2), lfo4v: s(0.6, 0.5),
        headMag: [0.6 + 0.3 * Math.abs(s(1.1)), 0.4 + 0.25 * Math.abs(s(0.8, 1)),
                  0.3 + 0.2 * Math.abs(s(0.6, 2)), 0.0],
        peak: { in: -8, delay: -12, reverb: -16, out: -6,
                l: -7 + 5 * Math.abs(s(1.9)), r: -9 + 5 * Math.abs(s(1.6, 1)) },
      };
      emit('levels', lvl);
      animRAF = requestAnimationFrame(step);
    };
    animRAF = requestAnimationFrame(step);
  }
  function stopLevelAnimation() { if (animRAF) cancelAnimationFrame(animRAF); animRAF = 0; }

  // --- native functions (preset / IR listers, reload) ---
  const FACTORY_PRESETS = ['Init', 'Dub Chamber', 'Ambient Wash', 'Vintage Tape',
    'Wide Space', 'Slapback', 'Cosmic Shimmer', 'Gated Verb', 'Tape Warble', 'Cathedral'];
  const USER_PRESETS = ['My Patch', 'Live Set A'];
  const FACTORY_IRS = ['Concert Hall', 'Plate 1', 'Spring Tank', 'Cathedral', 'Small Room', 'EMT 140'];
  function getNativeFunction(name) {
    const fns = {
      listFactoryPresets: () => Promise.resolve(FACTORY_PRESETS),
      listUserPresets: () => Promise.resolve(USER_PRESETS),
      listFactoryIRs: () => Promise.resolve(FACTORY_IRS),
      reloadUI: () => Promise.resolve(),
    };
    return fns[name] || (() => Promise.resolve(null));
  }

  global.Juce = {
    getSliderState, getToggleState, getComboBoxState,
    backend, getNativeFunction,
  };
  // Expose the animation hooks for the standalone/preview harness.
  global.__doobieStub = { startLevelAnimation, stopLevelAnimation, emit };
})(window);
