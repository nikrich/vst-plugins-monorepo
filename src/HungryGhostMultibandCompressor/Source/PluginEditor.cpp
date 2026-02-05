#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <Charts/MBCLineChart.h>
#include <Foundation/Typography.h>

using CommonUI::Charts::MBCLineChart;

HungryGhostMultibandCompressorAudioProcessorEditor::HungryGhostMultibandCompressorAudioProcessorEditor(HungryGhostMultibandCompressorAudioProcessor& p)
: juce::AudioProcessorEditor(&p), proc(p)
{
    setOpaque(true);

    // Create and configure chart BEFORE calling setSize (which triggers resized())
    chart = std::make_unique<MBCLineChart>();
    chart->setXRangeHz(20.0f, 20000.0f);
    // Make spectrum appear larger: tighter dB window centered near 0 dB
    chart->setYRangeDb(-36.0f, 12.0f);

    // Remove bottom compressor curve chart; we'll use knobs instead

    // Band selector
    bandLabel.setText("Band", juce::dontSendNotification);
    ui::foundation::Typography::apply(bandLabel, ui::foundation::Typography::Style::Subtitle);
    bandSel.addItem("1", 1);
    bandSel.addItem("2", 2);

    // Band selection handled later to reattach knob attachments

    // Chart drags -> write back to APVTS for the selected band
    auto idFor = [this](const char* name)
    {
        const int b = juce::jlimit(1, 2, bandSel.getSelectedId() > 0 ? bandSel.getSelectedId() : 1);
        return juce::String("band.") + juce::String(b) + "." + name;
    };
    // compChart removed; APVTS writes happen via knob attachments

    // Remove old per-band vertical sliders row (replaced by knobs)

    // Add chart and selector
    addAndMakeVisible(*chart);
    addAndMakeVisible(bandLabel);
    addAndMakeVisible(bandSel);

    // Enable overlay on the spectrum chart so it mimics Pro-Q aesthetics
    chart->enableOverlay(true);
    chart->setShowPrimaries(true); // Show the two primary compressor bands as interactive nodes

    // Configure bottom knobs (filmstrip-backed if available)
    auto setupRotary = [&](juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        s.setLookAndFeel(&donutLNF);
    };
    setupRotary(knobThresh); setupRotary(knobAttack); setupRotary(knobRelease);
    setupRotary(knobKnee); setupRotary(knobRatio); setupRotary(knobMix); setupRotary(knobOutput);

    addAndMakeVisible(knobThresh);
    addAndMakeVisible(knobAttack);
    addAndMakeVisible(knobRelease);
    addAndMakeVisible(knobKnee);
    addAndMakeVisible(knobRatio);
    addAndMakeVisible(knobMix);
    addAndMakeVisible(knobOutput);

    // Helper to (re)attach band-specific knobs based on current band selection
    auto attachBandKnobs = [this](int band)
    {
        const auto id = [band](const char* name){ return juce::String("band.") + juce::String(band) + "." + name; };
        attThresh.reset(); attAttack.reset(); attRelease.reset(); attKnee.reset(); attRatio.reset(); attMix.reset();
        attThresh = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id("threshold_dB"), knobThresh);
        attAttack = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id("attack_ms"),    knobAttack);
        attRelease= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id("release_ms"),   knobRelease);
        attKnee   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id("knee_dB"),      knobKnee);
        attRatio  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id("ratio"),        knobRatio);
        attMix    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id("mix_pct"),      knobMix);
    };

    // Attach output trim (global)
    attOutput = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "global.outputTrim_dB", knobOutput);

    // Initial band knob attachments
    attachBandKnobs(1);

    // Update attachments when band selection changes
    bandSel.onChange = [this, attachBandKnobs]
    {
        const int b = juce::jlimit(1, 2, bandSel.getSelectedId() > 0 ? bandSel.getSelectedId() : 1);
        attachBandKnobs(b);
    };

    // Default band and initial sync
    bandSel.setSelectedId(1, juce::dontSendNotification);
    bandSel.onChange();

    // Wire overlay callbacks to APVTS
    chart->onChangeXover = [this](float hz)
    {
        if (auto* p = proc.apvts.getParameter("xover.1.Hz"))
            p->setValueNotifyingHost(p->convertTo0to1(hz));
    };
    chart->onChangeThreshold = [this](int band, float th)
    {
        const char* id = (band==0? "band.1.threshold_dB" : "band.2.threshold_dB");
        if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(th));
    };
    chart->onChangeRatio = [this](int band, float ra)
    {
        const char* id = (band==0? "band.1.ratio" : "band.2.ratio");
        if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(ra));
    };
    chart->onChangeKnee = [this](int band, float k)
    {
        const char* id = (band==0? "band.1.knee_dB" : "band.2.knee_dB");
        if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(k));
    };

    // Defer setSize until after children are constructed
    setSize(900, 640);

    // Selected-node knobs configuration
    auto cfgKnob = [&](juce::Slider& s){ s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18); };
    cfgKnob(selFreq); cfgKnob(selThresh); cfgKnob(selRatio); cfgKnob(selKnee);
    selType.addItemList({"Bell","Low Shelf","High Shelf","Low Pass","High Pass","Notch"}, 1);
    addAndMakeVisible(selFreq); addAndMakeVisible(selThresh); addAndMakeVisible(selRatio); addAndMakeVisible(selKnee); addAndMakeVisible(selType);

    // Ranges
    {
        juce::NormalisableRange<double> fRange(20.0, 20000.0, 0.0, 0.3); selFreq.setNormalisableRange(fRange); selFreq.setValue(200.0);
        selThresh.setRange(-60.0, 12.0, 0.01); selThresh.setValue(-18.0);
        selRatio.setRange(1.0, 20.0, 0.01); selRatio.setValue(2.0);
        selKnee.setRange(0.0, 24.0, 0.01); selKnee.setValue(6.0);
    }

    // Knobs -> selected band (and APVTS for primaries)
    selFreq.onValueChange = [this]
    {
        chart->setSelectedBandValue(CommonUI::Charts::MBCLineChart::BandParam::Freq, (float) selFreq.getValue());
    };
    selThresh.onValueChange = [this]
    {
        chart->setSelectedBandValue(CommonUI::Charts::MBCLineChart::BandParam::Threshold, (float) selThresh.getValue());
    };
    selRatio.onValueChange = [this]
    {
        chart->setSelectedBandValue(CommonUI::Charts::MBCLineChart::BandParam::Ratio, (float) selRatio.getValue());
    };
    selKnee.onValueChange = [this]
    {
        chart->setSelectedBandValue(CommonUI::Charts::MBCLineChart::BandParam::Knee, (float) selKnee.getValue());
    };
    selType.onChange = [this]
    {
        chart->setSelectedBandValue(CommonUI::Charts::MBCLineChart::BandParam::Type, (float)(selType.getSelectedId()-1));
    };

    // Keep knob values synced on selection change
    chart->onSelectionChanged = [this](int){
        auto si = chart->getSelectedInfo();
        if (si.index >= 0)
        {
            selFreq.setValue(si.freqHz, juce::dontSendNotification);
            selThresh.setValue(si.thresholdDb, juce::dontSendNotification);
            selRatio.setValue(si.ratio, juce::dontSendNotification);
            selKnee.setValue(si.kneeDb, juce::dontSendNotification);
            selType.setSelectedId((int)si.type + 1, juce::dontSendNotification);
        }
    };

    // Decorative bands -> wire into EQ APVTS (eq.1..eq.N)
    chart->onDecorChanged = [this](int idx, float f, float gdb, float q)
    {
        const int bi = idx + 1; // map to 1-based eq band index
        const juce::String pfx = "eq." + juce::String(bi) + ".";
        auto set = [&](const juce::String& id, float v)
        {
            if (auto* p = proc.apvts.getParameter(pfx + id))
                p->setValueNotifyingHost(p->convertTo0to1(v));
        };
        // Enable and set type to Bell
        if (auto* p = proc.apvts.getParameter(pfx + "enabled")) p->setValueNotifyingHost(1.0f);
        if (auto* p = proc.apvts.getParameter(pfx + "type"))    p->setValueNotifyingHost(p->convertTo0to1(0.0f));
        set("freq_hz", f);
        set("gain_db", gdb);
        set("q", q);
    };

    startTimerHz(30);
}

HungryGhostMultibandCompressorAudioProcessorEditor::~HungryGhostMultibandCompressorAudioProcessorEditor()
{
    // Ensure no further timer callbacks can run while members are being destroyed
    stopTimer();

    // Explicitly release chart to avoid use-after-free if any async paints are queued
    chart.reset();
}

void HungryGhostMultibandCompressorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0D0D10));
}

void HungryGhostMultibandCompressorAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(12);

    // Top spectrum chart — increase height for a larger waveform
    auto top = r.removeFromTop(320);
    if (chart)
        chart->setBounds(top);

    // Band selector row
    auto selRow = r.removeFromTop(28);
    bandLabel.setBounds(selRow.removeFromLeft(60));
    bandSel.setBounds(selRow.removeFromLeft(80).reduced(0, 2));

    // Knob row
    auto row = r.removeFromTop(160).reduced(12);
    auto place = [&](juce::Slider& k, const char* label)
    {
        auto cw = row.removeFromLeft(row.getWidth() / (r.getWidth() > 0 ? 7 : 7));
        k.setBounds(cw.removeFromTop(120));
        // simple label under each knob
        juce::Label* lbl = new juce::Label({}, label);
        ui::foundation::Typography::apply(*lbl, ui::foundation::Typography::Style::Caption);
        addAndMakeVisible(lbl);
        lbl->setBounds(cw.removeFromTop(18));
    };
    place(knobThresh,  "Threshold");
    place(knobAttack,  "Attack");
    place(knobRelease, "Release");
    place(knobKnee,    "Knee");
    place(knobRatio,   "Ratio");
    place(knobMix,     "Mix");
    place(knobOutput,  "Output");

    // Selected-node knob strip (keep small strip for frequency and selected-band fine control)
    auto knobs = r.reduced(8);
    const int kw = knobs.getWidth() / 5;
    selFreq.setBounds(knobs.removeFromLeft(kw));
    selThresh.setBounds(knobs.removeFromLeft(kw));
    selRatio.setBounds(knobs.removeFromLeft(kw));
    selKnee.setBounds(knobs.removeFromLeft(kw));
    selType.setBounds(knobs.removeFromLeft(kw).reduced(8, 36));
}

void HungryGhostMultibandCompressorAudioProcessorEditor::timerCallback()
{
    // Analyzer: pull samples, compute FFT magnitude in dB for spectrum
    static std::vector<float> timeBuf; static std::vector<float> specPre; static std::vector<float> specPost;
    const int N = 1024; // small FFT for UI
    if ((int) timeBuf.size() < N) { timeBuf.assign(N, 0.0f); specPre.assign(N/2, -120.0f); specPost.assign(N/2, -120.0f); }

    auto computeMag = [&](const float* time, int count, std::vector<float>& out){
        juce::dsp::FFT fft((int) std::log2(N));
        std::vector<float> window(N, 0.0f);
        for (int i=0;i<N;++i) window[i] = 0.5f*(1.0f - std::cos(2.0f*juce::MathConstants<float>::pi*i/(N-1)));
        std::vector<float> fftBuf(2*N, 0.0f);
        const int used = juce::jlimit(1, N, count);
        for (int i=0;i<used; ++i) fftBuf[i*2] = time[i] * window[i];
        fft.performRealOnlyForwardTransform(fftBuf.data());
        const float scaleDiv = (float) juce::jmax(used, 1);
        for (int i=1;i<N/2;++i)
        {
            const float re = fftBuf[2*i]; const float im = fftBuf[2*i+1];
            const float mag = std::sqrt(re*re + im*im) / scaleDiv;
            out[(size_t)i] = juce::Decibels::gainToDecibels(std::max(mag, 1.0e-6f)) - 6.0f;
        }
    };

    int readPre = proc.readAnalyzerPre(timeBuf.data(), N);
    if (readPre > 0) { computeMag(timeBuf.data(), readPre, specPre); }

    int readPost = proc.readAnalyzerPost(timeBuf.data(), N);
    if (readPost > 0) { computeMag(timeBuf.data(), readPost, specPost); }

    const float effMaxHz = (float) (proc.getSampleRate() * 0.5 / juce::jmax(1, proc.getAnalyzerDecimate()));
    if (readPre > 0) chart->setSpectrum(specPre, 20.0f, effMaxHz);
    if (readPost > 0) chart->setPostSpectrum(specPost, 20.0f, effMaxHz);

    // Crossovers + GR overlay
    std::vector<float> fcs; fcs.push_back(proc.apvts.getRawParameterValue("xover.1.Hz")->load());
    chart->setCrossovers(fcs);
    std::vector<float> gr = { proc.getBandGrDb(0), proc.getBandGrDb(1) };
    chart->setGRdB(gr);

    // Keep compressor chart and overlay synced with current band params
    const int b = juce::jlimit(1, 2, bandSel.getSelectedId() > 0 ? bandSel.getSelectedId() : 1);
    const auto id = [b](const char* name){ return juce::String("band.") + juce::String(b) + "." + name; };
    auto rp = [&](const juce::String& pid){ return proc.apvts.getRawParameterValue(pid)->load(); };
    // No compChart anymore
    chart->setPrimaryBands(
        proc.apvts.getRawParameterValue("xover.1.Hz")->load(),
        proc.apvts.getRawParameterValue("band.1.threshold_dB")->load(),
        proc.apvts.getRawParameterValue("band.1.ratio")->load(),
        proc.apvts.getRawParameterValue("band.1.knee_dB")->load(),
        proc.apvts.getRawParameterValue("band.2.threshold_dB")->load(),
        proc.apvts.getRawParameterValue("band.2.ratio")->load(),
        proc.apvts.getRawParameterValue("band.2.knee_dB")->load());
}
