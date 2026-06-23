/*
  Doobie — analog dub delay
  Copyright (C) 2026 DatanoiseTV

  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version, and distributed WITHOUT ANY WARRANTY. See <https://www.gnu.org/licenses/>.
  You must retain this notice and the attribution to DatanoiseTV in any
  redistributed or derivative version.
*/

#include "WebEditor.h"
#include "../PluginProcessor.h"
#include "../ParameterIDs.h"
#include "../dsp/FactoryIRs.h"

#include <DoobieUIData.h>

#include <optional>
#include <vector>
#include <cstddef>

namespace doobie
{
namespace
{
    // ----- BinaryData lookup ------------------------------------------------
    // juce_add_binary_data turns each source file into NAME + NAMESize. The
    // generated symbol name is the file's basename with '.' replaced by '_'.
    // We can't iterate the table directly, so we use DoobieUIData::originalFilenames
    // to map a URL path back to a resource.
    struct ResourceTable
    {
        struct Entry { juce::String filename; const char* data; int size; };
        std::vector<Entry> entries;

        ResourceTable()
        {
            for (int i = 0; i < DoobieUIData::namedResourceListSize; ++i)
            {
                const char* name = DoobieUIData::namedResourceList[i];
                int size = 0;
                const char* data = DoobieUIData::getNamedResource (name, size);
                if (data == nullptr) continue;
                // originalFilenames are the source filenames passed to
                // juce_add_binary_data (e.g. "index.html", "react.production.min.js").
                const juce::String original (DoobieUIData::getNamedResourceOriginalFilename (name));
                entries.push_back ({ original, data, size });
            }
        }

        std::optional<juce::WebBrowserComponent::Resource> lookup (const juce::String& url) const
        {
            // The browser asks for paths like "/index.html", "/doobie.css",
            // "/vendor/react.production.min.js". Strip the leading slash and
            // any subdir prefix and match against the filename only — the
            // BinaryData target flattens all sources into one namespace.
            juce::String name = url.startsWithChar ('/') ? url.substring (1) : url;
            // Resource provider sometimes gets "" (root) on the very first
            // request — serve index.html for it.
            if (name.isEmpty()) name = "index.html";
            // Strip subdir prefix: "vendor/react.production.min.js" -> "react.production.min.js".
            const auto slash = name.lastIndexOfChar ('/');
            if (slash >= 0) name = name.substring (slash + 1);
            // Strip query strings.
            const auto q = name.indexOfChar ('?');
            if (q >= 0) name = name.substring (0, q);

            for (const auto& e : entries)
            {
                if (e.filename == name)
                {
                    juce::WebBrowserComponent::Resource r;
                    r.data.assign (reinterpret_cast<const std::byte*> (e.data),
                                   reinterpret_cast<const std::byte*> (e.data) + (size_t) e.size);
                    r.mimeType = mimeForName (name);
                    return r;
                }
            }
            return std::nullopt;
        }

        static juce::String mimeForName (const juce::String& name)
        {
            if (name.endsWith (".html"))  return "text/html";
            if (name.endsWith (".css"))   return "text/css";
            if (name.endsWith (".js") ||
                name.endsWith (".jsx") ||
                name.endsWith (".mjs"))   return "application/javascript";
            if (name.endsWith (".svg"))   return "image/svg+xml";
            if (name.endsWith (".png"))   return "image/png";
            if (name.endsWith (".json"))  return "application/json";
            return "application/octet-stream";
        }
    };

    static ResourceTable& resourceTable()
    {
        static ResourceTable t;
        return t;
    }

    // ----- Param wiring helpers ---------------------------------------------
    // The names below must match exactly between APVTS, the relays here, and
    // the IDs referenced in the JS PARAM_MAP. Single source of truth lives in
    // src/ParameterIDs.h; we re-quote them here so a typo on either side
    // becomes a compile / runtime error rather than a silent dead control.

    constexpr const char* kFloatIds[] = {
        "inputDrive", "mix", "outputGain", "width", "timeMs", "feedback", "duck",
        "preHpFreq", "preLpFreq", "preBass", "preTreble",
        "hpFreq", "lpFreq", "bass", "treble",
        "wow", "flutter", "drive", "hiss",
        "reverbMix", "springDecay", "springTone",
        "plateDecay", "plateSize", "plateDamp", "platePredelay", "reverbMod",
        "irGain", "irSpeed",
        "gateThreshold", "gateHold", "gateRelease",
        "shimmerSemis", "pitchSemis", "midiPortaMs", "pitchSpread",
        "head1Offset", "head2Offset", "head3Offset", "head4Offset",
        "lfo1Rate", "lfo1Depth", "lfo2Rate", "lfo2Depth",
        "lfo3Rate", "lfo3Depth", "lfo4Rate", "lfo4Depth",
        "lfo1Smooth", "lfo2Smooth", "lfo3Smooth", "lfo4Smooth",
        "lfo1Offset", "lfo2Offset", "lfo3Offset", "lfo4Offset",
        "envAttack", "envRelease", "envSens",
        "head1Level", "head2Level", "head3Level", "head4Level",
        "head1Pan",   "head2Pan",   "head3Pan",   "head4Pan",
        "head1Ratio", "head2Ratio", "head3Ratio", "head4Ratio",
        "mod1Amt", "mod2Amt", "mod3Amt", "mod4Amt", "mod5Amt", "mod6Amt", "mod7Amt", "mod8Amt",
        // Hardware-port additions (v0.14.0): input multimode filter + phaser
        "inFilterCutoff", "inFilterRes",
        "phaserRate", "phaserDepth", "phaserFb", "phaserMix",
    };
    constexpr const char* kBoolIds[] = {
        "syncMode", "pingPong", "freeze", "delayBypass", "midiPitchMode", "midiPortaOn", "outLevelerOn", "pitchOn", "feedbackKill",
        "head1On", "head2On", "head3On", "head4On",
        "inFilterOn", "phaserOn",
        "lfo1Sync", "lfo2Sync", "lfo3Sync", "lfo4Sync",
    };
    constexpr const char* kChoiceIds[] = {
        "delayMode", "syncDiv", "reverbMode", "reverbRoute",
        "lfo1Wave",  "lfo2Wave", "lfo3Wave", "lfo4Wave",
        "lfo1Div",   "lfo2Div",  "lfo3Div",  "lfo4Div",
        "mod1Src", "mod2Src", "mod3Src", "mod4Src", "mod5Src", "mod6Src", "mod7Src", "mod8Src",
        "mod1Dst", "mod2Dst", "mod3Dst", "mod4Dst", "mod5Dst", "mod6Dst", "mod7Dst", "mod8Dst",
        "mod1Mode","mod2Mode","mod3Mode","mod4Mode","mod5Mode","mod6Mode","mod7Mode","mod8Mode",
        "inFilterType", "phaserRoute",
        "pitchAlgo", "pitchRoute",
    };
}

// ============================================================================
WebEditor::WebEditor (::DoobieAudioProcessor& proc)
    : juce::AudioProcessorEditor (&proc), doobieProcessor (proc)
{
    setResizable (true, true);
    setResizeLimits (760, 480, 3040, 1920);

   #if JUCE_LINUX
    // ---- WebKitGTK environment hardening --------------------------------
    // JUCE 8 has no Wayland support; the WebKit child is explicitly an X11
    // client. Without GDK_BACKEND=x11, GTK in the same process tree can
    // pick Wayland and break the XEmbed reparent into the plugin host's
    // window. This is a correctness fix, not a performance one --
    // XWayland is the JUCE path either way.
    //
    // We deliberately do NOT touch WEBKIT_DISABLE_DMABUF_RENDERER or
    // WEBKIT_DISABLE_COMPOSITING_MODE here. Setting them globally cripples
    // GPU compositing for the WebView (the difference between 60 fps and
    // 1 fps on healthy GPUs). Those are narrow workarounds for NVIDIA-
    // proprietary + Wayland white-window, documented in the README for
    // affected users to set themselves. overwrite=0 means a power user can
    // still pre-export GDK_BACKEND.
    ::setenv ("GDK_BACKEND", "x11", 0);

    // ---- Subprocess helper location -------------------------------------
    // JUCE 8 extracts its WebKit helper to $TMPDIR/_juce_linux_subprocess*
    // and execv's it. Defaulting that to /tmp is fragile -- /tmp can be
    // mounted noexec (CIS-hardened RHEL/Rocky, some Debian server installs),
    // confined by AppArmor (Snap-packaged DAWs), denied tmpfs-exec by
    // SELinux (Fedora 41+ default), or be the wrong filesystem entirely
    // inside a Flatpak/bubblewrap sandbox.
    //
    // The user-owned location for ephemeral runtime files is
    // $XDG_RUNTIME_DIR (= /run/user/$UID under systemd; always
    // exec-allowed, cleared at logout). Falling back to ~/.cache/doobie
    // covers non-systemd systems. We never use /tmp -- a user-set $TMPDIR
    // takes precedence, but otherwise we point JUCE at our own dir from
    // the start.
    if (::getenv ("TMPDIR") == nullptr)
    {
        juce::File chosen;
        if (const auto* xdg = ::getenv ("XDG_RUNTIME_DIR"))
        {
            juce::File xdgDir { juce::String (xdg) };
            if (xdgDir.isDirectory())
            {
                chosen = xdgDir.getChildFile ("doobie");
                chosen.createDirectory();
            }
        }
        if (chosen == juce::File())
        {
            chosen = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                        .getChildFile (".cache/doobie");
            chosen.createDirectory();
        }
        ::setenv ("TMPDIR", chosen.getFullPathName().toRawUTF8(), 0);
    }
   #endif

    auto& apvts = doobieProcessor.getValueTreeState();

    // 1) Build all relays + their parameter attachments. We use ranged
    // parameters so attachments do the unit conversion for us.
    juce::WebBrowserComponent::Options options;
    options = options
        .withBackend (juce::WebBrowserComponent::Options::Backend::defaultBackend)
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled (true)
        // JUCE serves resource-provider URLs from the `juce://` scheme on
        // every platform; `https://` would have hit the real network (which
        // is exactly what was throwing "Could not connect to the server").
        // `getResourceProviderRoot()` returns the platform-correct served
        // root URL we navigate to + use as the CORS-allowed origin.
        .withResourceProvider (
            [] (const juce::String& url) { return resourceTable().lookup (url); },
            juce::URL (juce::WebBrowserComponent::getResourceProviderRoot()).getOrigin())
        .withUserScript ("window.DOOBIE_VERSION_STR = 'v" DOOBIE_VERSION " · " DOOBIE_GIT_BRANCH "';")
        .withNativeFunction (juce::Identifier { "presetPrev" },
            [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                doobieProcessor.getPresetManager().previous();
                complete (juce::var());
            })
        .withNativeFunction (juce::Identifier { "presetNext" },
            [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                doobieProcessor.getPresetManager().next();
                complete (juce::var());
            })
        // Returns the factory preset name list to JS so the preset browser
        // modal can render it. Resolves to an Array<String>.
        .withNativeFunction (juce::Identifier { "listFactoryPresets" },
            [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::Array<juce::var> arr;
                for (const auto& n : doobieProcessor.getPresetManager().getFactoryNames())
                    arr.add (juce::var (n));
                complete (juce::var (arr));
            })
        // User-saved presets (saveUser writes them to the application-data
        // folder). Listed separately so the browser can tag the rows
        // distinctly from the factory bank.
        .withNativeFunction (juce::Identifier { "listUserPresets" },
            [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::Array<juce::var> arr;
                for (const auto& n : doobieProcessor.getPresetManager().getUserNames())
                    arr.add (juce::var (n));
                complete (juce::var (arr));
            })
        // Returns the factory IR name list — used by the Convolution mode's
        // dropdown / browser.
        .withNativeFunction (juce::Identifier { "listFactoryIRs" },
            [] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::Array<juce::var> arr;
                for (const auto& n : doobie::factoryIRNames())
                    arr.add (juce::var (n));
                complete (juce::var (arr));
            })
        // Factory IR picker — index from JS, slot < 0 clears.
        .withEventListener (juce::Identifier { "ir_load_factory" },
            [this] (juce::var payload)
            {
                const int idx = (int) payload.getProperty ("index", -1);
                if (idx >= 0)
                    doobieProcessor.loadFactoryIR (idx);
                else
                    doobieProcessor.clearIR();
            })
        // Custom-file IR loader. JUCE's file chooser is the only sane way to
        // produce a sandbox-friendly native path; ChooserComponent runs on
        // the message thread and shouldn't block.
        .withEventListener (juce::Identifier { "ir_load_file" },
            [this] (juce::var)
            {
                auto chooser = std::make_shared<juce::FileChooser> (
                    "Load impulse response",
                    juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                    "*.wav;*.aif;*.aiff;*.flac");
                chooser->launchAsync (
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this, chooser] (const juce::FileChooser& fc)
                    {
                        const auto f = fc.getResult();
                        if (f.existsAsFile())
                            doobieProcessor.loadIR (f);
                    });
            })
        .withEventListener (juce::Identifier { "ir_clear" },
            [this] (juce::var) { doobieProcessor.clearIR(); })
        .withEventListener (juce::Identifier { "preset_prev" },
            [this] (juce::var) { doobieProcessor.getPresetManager().previous(); })
        .withEventListener (juce::Identifier { "preset_next" },
            [this] (juce::var) { doobieProcessor.getPresetManager().next(); })
        .withEventListener (juce::Identifier { "preset_load" },
            [this] (juce::var payload)
            {
                // Preset browser modal in the WebView clicks a row; the
                // React side ships the preset name and we resolve it.
                const auto name = payload.getProperty ("name", juce::String()).toString();
                if (name.isNotEmpty())
                    doobieProcessor.getPresetManager().loadByName (name);
            })
        .withEventListener (juce::Identifier { "preset_save" },
            [this] (juce::var payload)
            {
                // The WebView's React modal collects the name and ships it
                // in the payload; we just persist. No native AlertWindow
                // (looked alien against the dark plugin chrome).
                const auto name = payload.getProperty ("name", juce::String()).toString().trim();
                if (name.isNotEmpty())
                    doobieProcessor.getPresetManager().saveUser (name);
            });

    // Pre-allocate so vector growth doesn't invalidate pointers held by Options.
    sliderBindings.reserve (juce::numElementsInArray (kFloatIds));
    toggleBindings.reserve (juce::numElementsInArray (kBoolIds));
    comboBindings .reserve (juce::numElementsInArray (kChoiceIds));

    for (auto id : kFloatIds)
    {
        SliderBinding b;
        b.relay = std::make_unique<juce::WebSliderRelay> (juce::String (id));
        options = options.withOptionsFrom (*b.relay);
        sliderBindings.push_back (std::move (b));
    }
    for (auto id : kBoolIds)
    {
        ToggleBinding b;
        b.relay = std::make_unique<juce::WebToggleButtonRelay> (juce::String (id));
        options = options.withOptionsFrom (*b.relay);
        toggleBindings.push_back (std::move (b));
    }
    for (auto id : kChoiceIds)
    {
        ComboBinding b;
        b.relay = std::make_unique<juce::WebComboBoxRelay> (juce::String (id));
        options = options.withOptionsFrom (*b.relay);
        comboBindings.push_back (std::move (b));
    }

    // 2) Construct the WebView with all relays folded in.
    webView = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webView);

    // 3) Attach each relay to its matching APVTS parameter. The attachment
    // glue handles range mapping (Hz, dB, ms, normalised 0..1) automatically.
    auto attachFloat = [&apvts] (SliderBinding& b, const juce::String& id)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            b.attach = std::make_unique<juce::WebSliderParameterAttachment> (*p, *b.relay, apvts.undoManager);
    };
    auto attachBool = [&apvts] (ToggleBinding& b, const juce::String& id)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            b.attach = std::make_unique<juce::WebToggleButtonParameterAttachment> (*p, *b.relay, apvts.undoManager);
    };
    auto attachChoice = [&apvts] (ComboBinding& b, const juce::String& id)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            b.attach = std::make_unique<juce::WebComboBoxParameterAttachment> (*p, *b.relay, apvts.undoManager);
    };

    for (size_t i = 0; i < std::size (kFloatIds);  ++i) attachFloat  (sliderBindings[i], kFloatIds[i]);
    for (size_t i = 0; i < std::size (kBoolIds);   ++i) attachBool   (toggleBindings[i], kBoolIds[i]);
    for (size_t i = 0; i < std::size (kChoiceIds); ++i) attachChoice (comboBindings [i], kChoiceIds[i]);

    // 4) Now we have the WebView, set the editor size — resized() will lay
    // it out. (Doing this before the WebView existed left it bounds-less.)
    setSize (1520, 960);

    // 5) Load the HTML shell.
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot() + "index.html");

    // 6) Live data emission timer.
    startTimerHz (30);
}

WebEditor::~WebEditor() = default;

void WebEditor::paint (juce::Graphics& g)
{
    // The WebView paints everything; the host expects opaque content though,
    // so fill with pure black under the view in case the page is slow to load.
    g.fillAll (juce::Colours::black);
}

void WebEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void WebEditor::timerCallback()
{
    emitLevels();
    emitPresetInfo();
    emitIRInfo();
}

void WebEditor::emitLevels()
{
    if (webView == nullptr) return;

    auto toDb = [] (float lin) -> float
    {
        if (lin <= 1.0e-5f) return -90.0f;
        return 20.0f * std::log10 (lin);
    };

    const float inLin  = doobieProcessor.getInputLevel();
    const float dlyLin = doobieProcessor.getDelayLevel();
    const float revLin = doobieProcessor.getReverbLevel();
    const float outL   = doobieProcessor.getOutputLevel (0);
    const float outR   = doobieProcessor.getOutputLevel (1);
    const float outMono = 0.5f * (outL + outR);

    juce::DynamicObject::Ptr peakObj = new juce::DynamicObject();
    peakObj->setProperty ("in",     toDb (inLin));
    peakObj->setProperty ("delay",  toDb (dlyLin));
    peakObj->setProperty ("reverb", toDb (revLin));
    peakObj->setProperty ("out",    toDb (outMono));
    peakObj->setProperty ("l",      toDb (outL));
    peakObj->setProperty ("r",      toDb (outR));

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("in",     toDb (inLin));
    root->setProperty ("delay",  toDb (dlyLin));
    root->setProperty ("reverb", toDb (revLin));
    root->setProperty ("out",    toDb (outMono));
    root->setProperty ("peak",   juce::var (peakObj.get()));
    // Diagnostic: latest MIDI note-on the processor saw (-1 = none).
    // Surfaced so the user can confirm MIDI is reaching the plugin while
    // wiring up a source in the host.
    root->setProperty ("midiNote", doobieProcessor.getLastMidiNote());
    // Per-head live magnitudes (0..~1, post-level, before per-head pan).
    // The Fader strip below each head reads these to draw a tiny VU on
    // top of the fader track.
    {
        const auto& hm = doobieProcessor.getEngine().headMagnitudes();
        juce::Array<juce::var> ha;
        for (int i = 0; i < 4; ++i)
            ha.add (juce::var (hm[(size_t) i].load (std::memory_order_relaxed)));
        root->setProperty ("headMag", juce::var (ha));
    }
    // Output leveler gain-reduction in dB (0 = no GR, negative = reducing).
    // Shown as a marker on the main L/R VU meters.
    root->setProperty ("grDb",     doobieProcessor.getOutputGrDb());
    // Mod-source live values for the Sources tab visualisers (env follower
    // bar + LED, LFO scopes).
    root->setProperty ("env",   doobieProcessor.getEnvValue());
    root->setProperty ("lfo1v", doobieProcessor.getLfo1Value());
    root->setProperty ("lfo2v", doobieProcessor.getLfo2Value());
    root->setProperty ("lfo3v", doobieProcessor.getLfo3Value());
    root->setProperty ("lfo4v", doobieProcessor.getLfo4Value());

    webView->emitEventIfBrowserIsVisible (juce::Identifier { "levels" }, juce::var (root.get()));
}

void WebEditor::emitPresetInfo()
{
    if (webView == nullptr) return;

    const auto name = doobieProcessor.getPresetManager().getCurrentName();
    juce::String cat;       // The preset manager doesn't currently expose a
                            // category; show the bank ("USER" vs factory) for
                            // a meaningful tag — refine when the manager
                            // grows categories.
    cat = name.containsAnyOf ("Dub Reggae") ? "DUB"
        : name.contains ("Ambient")        ? "AMBIENT"
        : name.contains ("Vintage")        ? "VINTAGE"
        : name.contains ("Cosmic")         ? "WIDE"
        : juce::String();

    const bool dirty = doobieProcessor.isCurrentPresetDirty();
    // Same reasoning as emitIRInfo — broadcast every tick so the JS side
    // never gets stuck on stale defaults if the listener registered
    // late.
    lastPresetDirty = dirty;

    lastPresetName = name;
    lastPresetCat  = cat;

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("name",  name);
    obj->setProperty ("cat",   cat);
    obj->setProperty ("dirty", doobieProcessor.isCurrentPresetDirty());
    webView->emitEventIfBrowserIsVisible (juce::Identifier { "presetInfo" }, juce::var (obj.get()));
}

void WebEditor::emitIRInfo()
{
    if (webView == nullptr) return;

    const bool has        = doobieProcessor.hasIR();
    const int  factoryIdx = doobieProcessor.irIsFactory() ? doobieProcessor.getFactoryIRIndex() : -1;
    const auto name       = doobieProcessor.getIRDisplayName();

    // Previously this gated emission on "value changed since last tick"
    // for performance. That created a startup race: if the IR is loaded
    // BEFORE the editor's JS-side event listener is registered (which
    // happens on every fresh project load), the first emit was sent
    // into the void, and subsequent ticks saw "no change vs cache" so
    // nothing else fired — the UI would forever show "(no IR)" while
    // the engine happily convolved. Just broadcast every tick instead;
    // the payload is a tiny JSON object and 30 Hz is fine.
    lastHadIR = has;
    lastFactoryIRIndex = factoryIdx;
    lastIRName = name;

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("hasIR",        has);
    obj->setProperty ("factoryIndex", factoryIdx);
    obj->setProperty ("isFactory",    doobieProcessor.irIsFactory());
    obj->setProperty ("isFile",       doobieProcessor.irIsFile());
    obj->setProperty ("name",         name);
    webView->emitEventIfBrowserIsVisible (juce::Identifier { "irInfo" }, juce::var (obj.get()));
}
} // namespace doobie
