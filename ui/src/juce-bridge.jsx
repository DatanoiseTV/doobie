/* ============================================================
   Doobie · JUCE ↔ React bridge
   ============================================================
   Replaces React.useState with hooks that read live state from
   APVTS via the JUCE 8 WebSliderRelay / WebToggleButtonRelay /
   WebComboBoxRelay APIs. Two-way bound: the host can drive any
   knob (automation, preset load) and the UI reflects it; the
   user dragging a knob writes back through the relay so the
   audio engine sees the change.

   For native live data (digital VU levels, gate-envelope, mod
   indicator amounts) we listen to `Juce.backend.addEventListener`
   events the editor emits on a 30 Hz timer.
   ============================================================ */

(function (global) {
  const { useState, useEffect, useCallback, useRef } = React;

  // Slider relays publish a 0..1 normalised value. JUCE handles the
  // mapping back to the parameter's actual range (Hz, dB, ms etc).
  function useJuceSlider(id) {
    const relay = global.Juce.getSliderState(id);
    const [v, setV] = useState(relay.getNormalisedValue());
    useEffect(() => {
      const onChanged = () => setV(relay.getNormalisedValue());
      relay.valueChangedEvent.addListener(onChanged);
      // No removeListener API in JUCE web-relay; React will rebind on remount.
      return undefined;
    }, [id]);
    const set = useCallback((nv) => {
      const c = Math.max(0, Math.min(1, nv));
      relay.setNormalisedValue(c);
      setV(c);
    }, [id]);
    return [v, set];
  }

  function useJuceToggle(id) {
    const relay = global.Juce.getToggleState(id);
    const [v, setV] = useState(!!relay.getValue());
    useEffect(() => {
      const onChanged = () => setV(!!relay.getValue());
      relay.valueChangedEvent.addListener(onChanged);
      return undefined;
    }, [id]);
    const set = useCallback((b) => { relay.setValue(!!b); setV(!!b); }, [id]);
    return [v, set];
  }

  // Choice params: APVTS stores them as 0..N-1 integer indices. We expose
  // index <-> string conversion here so the design code can keep talking
  // in human-readable choice names without each panel re-mapping.
  function useJuceChoice(id, options) {
    const relay = global.Juce.getComboBoxState(id);
    const initialIdx = relay.getChoiceIndex();
    const [idx, setIdx] = useState(initialIdx);
    useEffect(() => {
      const onChanged = () => setIdx(relay.getChoiceIndex());
      relay.valueChangedEvent.addListener(onChanged);
      return undefined;
    }, [id]);
    const value = options[idx] || options[0];
    const setValue = useCallback((s) => {
      const ni = Math.max(0, options.indexOf(s));
      relay.setChoiceIndex(ni);
      setIdx(ni);
    }, [id, options]);
    return [value, setValue];
  }

  // Native events emitted by the editor on its 30 Hz UI timer. The payload
  // shape is documented in WebEditor.cpp -- this hook just receives + caches
  // the latest value, and re-renders any component using it.
  function useJuceEvent(name, initial) {
    const [v, setV] = useState(initial);
    useEffect(() => {
      const cb = (e) => setV(e);
      global.Juce.backend.addEventListener(name, cb);
      return () => global.Juce.backend.removeEventListener(name, cb);
    }, [name]);
    return v;
  }

  // ---- Compound hooks the design's components consume ---------------------

  // The mod matrix lives in N slots. Each slot has src/dst (choice) + amt
  // (slider). The DELAY panel needs to display "this knob is being
  // modulated, by this much" so we compute a per-destination amount map
  // here and expose it.
  // MUST mirror src/dsp/ModMatrix.h's ModSource / ModDest enums exactly --
  // the matrix combo box stores an integer index, so a mismatched name list
  // would route source/dest to the wrong slot silently. Keep this in sync
  // with modDestNames() in ModMatrix.h whenever destinations are added.
  // MUST match the ModSource enum order in src/dsp/ModMatrix.h.
  const MOD_SOURCES = ['Off', 'LFO 1', 'LFO 2', 'LFO 3', 'LFO 4', 'Env'];
  const MOD_DESTS   = [
    'Off',
    'Delay Time', 'Feedback', 'Mix', 'Width', 'Duck',
    'Drive', 'Wow', 'Flutter', 'Age',
    'Pre Low Cut', 'Pre High Cut', 'Low Cut', 'High Cut',
    'Bass', 'Treble',
    'Head 1 Level', 'Head 2 Level', 'Head 3 Level', 'Head 4 Level',
    'Reverb Mix', 'Reverb Mod',
    'Plate Decay', 'Plate Size', 'Plate Damp', 'Plate Predelay',
    'Spring Decay', 'Spring Tone', 'IR Gain',
    'Head 1 Pan', 'Head 2 Pan', 'Head 3 Pan', 'Head 4 Pan',
    'Head 1 Time', 'Head 2 Time', 'Head 3 Time', 'Head 4 Time',
    'In Filter Cutoff', 'In Filter Res',
    'Pan', 'Out Level',
    'Phaser Rate', 'Phaser Depth', 'Phaser Mix',
    'Input Gain',
  ];
  // Map from MOD_DEST string -> APVTS param IDs whose knob should display
  // a live mod-range arc + dot. Multiple IDs means the destination affects
  // more than one knob (e.g. Pan reaches all four head pans).
  const DEST_TO_PARAMS = {
    'Delay Time':       ['timeMs'],
    'Feedback':         ['feedback'],
    'Mix':              ['mix'],
    'Width':            ['width'],
    'Duck':             ['duck'],
    'Drive':            ['drive'],
    'Wow':              ['wow'],
    'Flutter':          ['flutter'],
    'Age':              ['hiss'],
    'Pre Low Cut':      ['preHpFreq'],
    'Pre High Cut':     ['preLpFreq'],
    'Low Cut':          ['hpFreq'],
    'High Cut':         ['lpFreq'],
    'Bass':             ['bass'],
    'Treble':           ['treble'],
    'Head 1 Level':     ['head1Level'],
    'Head 2 Level':     ['head2Level'],
    'Head 3 Level':     ['head3Level'],
    'Head 4 Level':     ['head4Level'],
    'Reverb Mix':       ['reverbMix'],
    'Reverb Mod':       ['reverbMod'],
    'Plate Decay':      ['plateDecay'],
    'Plate Size':       ['plateSize'],
    'Plate Damp':       ['plateDamp'],
    'Plate Predelay':   ['platePredelay'],
    'Spring Decay':     ['springDecay'],
    'Spring Tone':      ['springTone'],
    'IR Gain':          ['irGain'],
    'Head 1 Pan':       ['head1Pan'],
    'Head 2 Pan':       ['head2Pan'],
    'Head 3 Pan':       ['head3Pan'],
    'Head 4 Pan':       ['head4Pan'],
    'Head 1 Time':      ['head1Ratio'],
    'Head 2 Time':      ['head2Ratio'],
    'Head 3 Time':      ['head3Ratio'],
    'Head 4 Time':      ['head4Ratio'],
    'In Filter Cutoff': ['inFilterCutoff'],
    'In Filter Res':    ['inFilterRes'],
    // Synthetic mod-only destinations (no dedicated knob — the modulation
    // applies to a hidden EngineParams field). We still emit a row so
    // panels.jsx can light any knob that visually tracks them.
    'Pan':              ['headPan0', 'headPan1', 'headPan2', 'headPan3'],
    'Out Level':        ['outputGain'],
    'Phaser Rate':      ['phaserRate'],
    'Phaser Depth':     ['phaserDepth'],
    'Phaser Mix':       ['phaserMix'],
    'Input Gain':       ['inputDrive'],
  };

  // Subscribes to every slot's relays + the live LFO/env values published in
  // the host `levels` event, and returns two maps keyed by APVTS param id:
  //
  //   ranges[paramId] = half-range (0..1) — the maximum modulation amplitude
  //                     this destination will see (used to draw the mod ARC).
  //   live[paramId]   = live signed offset (-half..+half) — the instantaneous
  //                     modulation applied right now (used to position the
  //                     mod DOT so it actually traces the source waveform
  //                     instead of a generic sine tween).
  //
  // The returned object is still subscriptable as `mods[paramId]` (number =
  // ranges) for backwards-compat, but also has `.live[paramId]` for the dot.
  function useJuceModMap(numSlots) {
    const slots = [];
    for (let i = 0; i < numSlots; ++i) {
      const n = i + 1;
      const src = global.Juce.getComboBoxState(`mod${n}Src`);
      const dst = global.Juce.getComboBoxState(`mod${n}Dst`);
      const amt = global.Juce.getSliderState(`mod${n}Amt`);
      slots.push({ src, dst, amt });
    }
    // Per-LFO depth and env sens scale the source's effective range — same
    // factor the engine applies, so the on-screen arc width matches.
    const depths = [
      global.Juce.getSliderState('lfo1Depth'),
      global.Juce.getSliderState('lfo2Depth'),
      global.Juce.getSliderState('lfo3Depth'),
      global.Juce.getSliderState('lfo4Depth'),
      global.Juce.getSliderState('envSens'),
    ];

    const [tick, setTick] = useState(0);
    useEffect(() => {
      const bump = () => setTick(t => t + 1);
      slots.forEach(s => {
        s.src.valueChangedEvent.addListener(bump);
        s.dst.valueChangedEvent.addListener(bump);
        s.amt.valueChangedEvent.addListener(bump);
      });
      depths.forEach (d => d.valueChangedEvent.addListener(bump));
      return undefined;
    }, []);

    // Live source values come in via the `levels` event (lfo1v..lfo4v in
    // [-1,+1], env in [0,1]). The Knob refreshes on each `levels` tick at
    // 30 Hz so the dot follows the actual waveform.
    const lv = JuceBridge.useJuceEvent ('levels', { lfo1v:0, lfo2v:0, lfo3v:0, lfo4v:0, env:0 });
    // Map ModSource enum index to its live value. Order MUST match
    // ModMatrix.h::ModSource — Off=0, Lfo1..4=1..4, Env=5.
    const sourceLive = [0, lv.lfo1v || 0, lv.lfo2v || 0, lv.lfo3v || 0, lv.lfo4v || 0, lv.env || 0];

    const depthOf = (sIdx) =>
      sIdx >= 1 && sIdx <= 4 ? depths[sIdx - 1].getNormalisedValue()
      : sIdx === 5           ? depths[4].getNormalisedValue()
      : 0;

    const ranges = {};
    const live   = {};
    slots.forEach(s => {
      const si = s.src.getChoiceIndex();
      const di = s.dst.getChoiceIndex();
      if (si <= 0 || di <= 0) return;
      const amt01 = s.amt.getNormalisedValue();
      const amt   = amt01 * 2 - 1;
      const half  = Math.abs(amt) * depthOf(si) * 0.5;
      if (half <= 0) return;
      // Live offset: source × signed amount × depth × 0.5 (same shape the
      // engine applies, in normalised-knob space).
      // sourceLive[si] is already the engine's depth × shape (Lfo::advance
      // multiplies by depth before returning). Multiply only by amount ×
      // the 0.5 destination scaling — not by depth again. The previous
      // `* depthOf(si)` here applied depth twice, squashing the dot's
      // travel to depth² × amount × 0.5 and making it look like the
      // indicator wasn't moving even with a fully-set-up matrix slot.
      const offset = sourceLive[si] * amt * 0.5;
      const params = DEST_TO_PARAMS[MOD_DESTS[di]] || [];
      params.forEach(p => {
        ranges[p] = Math.max (ranges[p] || 0, half);
        live[p]   = (live[p]   || 0) + offset;
      });
    });
    // Backwards-compat: indexable like the old flat map (number = range).
    Object.assign (ranges, { live });
    return ranges;
  }

  global.JuceBridge = {
    useJuceSlider, useJuceToggle, useJuceChoice, useJuceEvent, useJuceModMap,
    MOD_SOURCES, MOD_DESTS,
  };
})(window);
