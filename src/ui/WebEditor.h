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

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>

// DoobieAudioProcessor lives in the global namespace (matches the historical
// `DoobieAudioProcessorEditor` shape). Forward-declare here so the WebEditor
// constructor can take a reference without dragging the full header into
// every translation unit that includes us.
class DoobieAudioProcessor;

namespace doobie
{

// JUCE 8 WebView-based editor. Hosts a WebBrowserComponent that loads the
// vendored HTML/CSS/JS bundle from BinaryData; every APVTS parameter is
// two-way bound to its matching DOM control via a WebSliderRelay /
// WebToggleButtonRelay / WebComboBoxRelay + the corresponding
// {Web,...}ParameterAttachment. Live metering + preset state are pushed to
// the JS side via emitEventIfBrowserIsVisible on a 30 Hz UI timer.
//
// No knob is rendered without a relay backing it, and no parameter exists
// without a knob bound to it (or, in the case of internal-only params like
// the IR file path, an explicit sidecar property). That contract is what
// the user means by "everything functional, no stubs".
class WebEditor : public juce::AudioProcessorEditor,
                  private juce::Timer
{
public:
    explicit WebEditor (::DoobieAudioProcessor& proc);
    ~WebEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // Live-data emission helpers. Called from timerCallback on the message
    // thread; the processor publishes the values on atomics so this read
    // is lock-free.
    void emitLevels();
    void emitPresetInfo();

    ::DoobieAudioProcessor& doobieProcessor;

    // Heap-allocated relay storage. We have many — one per APVTS parameter
    // — and can't enumerate them by index, so we keep them in vectors keyed
    // by name. CRITICAL: the relays are WebViewLifetimeListeners on the
    // browser. The browser's destructor walks its listener list and calls
    // each one — so the relays MUST outlive the browser. C++ destructs
    // members in reverse declaration order, so the bindings are declared
    // *before* `webView` here, which means they are destroyed AFTER it
    // (and the browser finds living listeners during teardown).
    struct SliderBinding   { std::unique_ptr<juce::WebSliderRelay>       relay; std::unique_ptr<juce::WebSliderParameterAttachment>       attach; };
    struct ToggleBinding   { std::unique_ptr<juce::WebToggleButtonRelay> relay; std::unique_ptr<juce::WebToggleButtonParameterAttachment> attach; };
    struct ComboBinding    { std::unique_ptr<juce::WebComboBoxRelay>     relay; std::unique_ptr<juce::WebComboBoxParameterAttachment>     attach; };

    std::vector<SliderBinding>  sliderBindings;
    std::vector<ToggleBinding>  toggleBindings;
    std::vector<ComboBinding>   comboBindings;

    // The WebView. Constructed after every relay so the relay options can
    // be folded into the WebBrowserComponent::Options; destructed first by
    // the rule above.
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // Last-known preset name + category, pushed only when changed (so we're
    // not spamming the JS side with redundant events).
    juce::String lastPresetName, lastPresetCat;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebEditor)
};
} // namespace doobie
