/* ============================================================
   Doobie · demo-props helper for preview cards
   ============================================================
   The panels take plain props (p / setP / heads / matrix / levels)
   that the real App derives from APVTS via bindParams/makeP etc.
   Those helpers are global function declarations (app.jsx) after
   concatenation, so we can reuse them verbatim against the stub
   bridge to hand each panel card the exact same populated state
   the full interface shows — no re-authored fixtures.
   ============================================================ */
(function (D) {
  const noop = () => {};
  let _p = null, _heads = null, _matrix = null;
  const buildP = () => (_p || (_p = makeP(bindParams(noop))));
  const buildHeads = () => (_heads || (_heads = makeHeads(bindHeads(noop))));
  const buildMatrix = () => (_matrix || (_matrix = makeMatrix(bindMatrix(noop))));

  // Static levels snapshot mirroring the stub's seeded frame — enough for
  // meters, VU strips, and mod dots to read as alive in a still card.
  const levels = {
    in: -14, delay: -18, reverb: -22, out: -12, midiNote: -1, grDb: 0, env: 0.35,
    lfo1v: 0.4, lfo2v: -0.2, lfo3v: 0.1, lfo4v: 0.3,
    headMag: [0.72, 0.5, 0.34, 0.0],
    peak: { in: -8, delay: -12, reverb: -16, out: -6, l: -7, r: -9 },
  };
  const stages = [
    { label: 'IN', base: 0.7, peakDb: -8 },
    { label: 'DELAY', base: 0.55, peakDb: -12 },
    { label: 'REVERB', base: 0.42, peakDb: -16 },
    { label: 'OUT', base: 0.62, peakDb: -6 },
  ];
  const presetInfo = { name: 'Dub Chamber', cat: 'DUB', dirty: false };
  const irInfo = { hasIR: true, factoryIndex: 0, isFactory: true, isFile: false, name: 'Concert Hall' };

  D.__demo = {
    get p() { return buildP(); },
    setP: noop,
    get heads() { return buildHeads(); },
    setHead: noop,
    get matrix() { return buildMatrix(); },
    setMx: noop,
    // Empty mod map: knobs simply show no modulation arc (clean default look).
    mods: Object.assign({}, { live: {} }),
    levels, stages, presetInfo, irInfo,
    noop,
  };
})(window.Doobie);
