#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <Charts/MBCLineChart.h>
#include <Charts/CompressorChart.h>

using CommonUI::Charts::MBCLineChart;

HungryGhostMultibandCompressorAudioProcessorEditor::HungryGhostMultibandCompressorAudioProcessorEditor(HungryGhostMultibandCompressorAudioProcessor& p)
: juce::AudioProcessorEditor(&p), proc(p)
{
    setOpaque(true);

    // Create and configure chart BEFORE calling setSize (which triggers resized())
    chart = std::make_unique<MBCLineChart>();
    chart->setXRangeHz(20.0f, 20000.0f);
    chart->setYRangeDb(-60.0f, 12.0f);

    // Interactive compressor curve chart
    compChart = std::make_unique<CommonUI::Charts::CompressorChart>();
    compChart->setRanges(-60.0f, 0.0f, -60.0f, 12.0f);

    // Band selector
    bandLabel.setText("Band", juce::dontSendNotification);
    bandLabel.setJustificationType(juce::Justification::centred);
    bandSel.addItem("1", 1);
    bandSel.addItem("2", 2);

    // When band changes, reflect current APVTS values in the chart
    bandSel.onChange = [this]
    {
        const int b = juce::jlimit(1, 2, bandSel.getSelectedId() > 0 ? bandSel.getSelectedId() : 1);
        auto rp = [&](const juce::String& id){ return proc.apvts.getRawParameterValue(id)->load(); };
        const auto id = [b](const char* name){ return juce::String("band.") + juce::String(b) + "." + name; };
        if (compChart)
        {
            compChart->setThresholdDb(rp(id("threshold_dB")));
            compChart->setRatio(rp(id("ratio")));
            compChart->setKneeDb(rp(id("knee_dB")));
        }
    };

    // Chart drags -> write back to APVTS for the selected band
    auto idFor = [this](const char* name)
    {
        const int b = juce::jlimit(1, 2, bandSel.getSelectedId() > 0 ? bandSel.getSelectedId() : 1);
        return juce::String("band.") + juce::String(b) + "." + name;
    };
    if (compChart)
    {
        compChart->onThresholdChanged = [this, idFor](float v)
        {
            if (auto* p = proc.apvts.getParameter(idFor("threshold_dB")))
                p->setValueNotifyingHost(p->convertTo0to1(v));
        };
        compChart->onRatioChanged = [this, idFor](float v)
        {
            if (auto* p = proc.apvts.getParameter(idFor("ratio")))
                p->setValueNotifyingHost(p->convertTo0to1(v));
        };
        compChart->onKneeChanged = [this, idFor](float v)
        {
            if (auto* p = proc.apvts.getParameter(idFor("knee_dB")))
                p->setValueNotifyingHost(p->convertTo0to1(v));
        };
    }

    // Controls row (threshold/ratio per band)
    auto configSlider = [](juce::Slider& s){ s.setSliderStyle(juce::Slider::LinearVertical); s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18); };
    configSlider(th1); configSlider(th2); configSlider(ra1); configSlider(ra2);
    lth1.setText("Thresh 1", juce::dontSendNotification);
    lth2.setText("Thresh 2", juce::dontSendNotification);
    lra1.setText("Ratio 1", juce::dontSendNotification);
    lra2.setText("Ratio 2", juce::dontSendNotification);
    lth1.attachToComponent(&th1, false); lth2.attachToComponent(&th2, false);
    lra1.attachToComponent(&ra1, false); lra2.attachToComponent(&ra2, false);

    addAndMakeVisible(th1); addAndMakeVisible(th2); addAndMakeVisible(ra1); addAndMakeVisible(ra2);

    // Add charts and selector
    addAndMakeVisible(*chart);
    addAndMakeVisible(bandLabel);
    addAndMakeVisible(bandSel);
    addAndMakeVisible(*compChart);

    // Enable overlay on the spectrum chart so it mimics Pro-Q aesthetics
    chart->enableOverlay(true);

    // Attach slider ranges & initial values from APVTS
    auto get = [&](const char* id){ return proc.apvts.getParameter(id); };
    if (auto* p = get("band.1.threshold_dB")) th1.setRange(p->getNormalisableRange().start, p->getNormalisableRange().end); th1.setValue(proc.apvts.getRawParameterValue("band.1.threshold_dB")->load());
    if (auto* p = get("band.2.threshold_dB")) th2.setRange(p->getNormalisableRange().start, p->getNormalisableRange().end); th2.setValue(proc.apvts.getRawParameterValue("band.2.threshold_dB")->load());
    if (auto* p = get("band.1.ratio"))       ra1.setRange(p->getNormalisableRange().start, p->getNormalisableRange().end); ra1.setValue(proc.apvts.getRawParameterValue("band.1.ratio")->load());
    if (auto* p = get("band.2.ratio"))       ra2.setRange(p->getNormalisableRange().start, p->getNormalisableRange().end); ra2.setValue(proc.apvts.getRawParameterValue("band.2.ratio")->load());

    th1.onValueChange = [this]{ if (auto* p = proc.apvts.getParameter("band.1.threshold_dB")) p->setValueNotifyingHost(p->convertTo0to1((float)th1.getValue())); };
    th2.onValueChange = [this]{ if (auto* p = proc.apvts.getParameter("band.2.threshold_dB")) p->setValueNotifyingHost(p->convertTo0to1((float)th2.getValue())); };
    ra1.onValueChange = [this]{ if (auto* p = proc.apvts.getParameter("band.1.ratio"))       p->setValueNotifyingHost(p->convertTo0to1((float)ra1.getValue())); };
    ra2.onValueChange = [this]{ if (auto* p = proc.apvts.getParameter("band.2.ratio"))       p->setValueNotifyingHost(p->convertTo0to1((float)ra2.getValue())); };

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

    startTimerHz(30);
}

HungryGhostMultibandCompressorAudioProcessorEditor::~HungryGhostMultibandCompressorAudioProcessorEditor()
{
    // Ensure no further timer callbacks can run while members are being destroyed
    stopTimer();

    // Clear any lambdas that capture `this` to avoid accidental late invocations
    th1.onValueChange = {};
    th2.onValueChange = {};
    ra1.onValueChange = {};
    ra2.onValueChange = {};

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

    // Top spectrum chart
    auto top = r.removeFromTop(240);
    if (chart)
        chart->setBounds(top);

    // Band selector row
    auto selRow = r.removeFromTop(28);
    bandLabel.setBounds(selRow.removeFromLeft(60));
    bandSel.setBounds(selRow.removeFromLeft(80).reduced(0, 2));

    // Compressor curve chart
    auto compArea = r.removeFromTop(220);
    if (compChart)
        compChart->setBounds(compArea);

    // Bottom controls row (existing)
    auto row = r.removeFromTop(100).reduced(8);
    const int w = row.getWidth() / 4;
    th1.setBounds(row.removeFromLeft(w));
    th2.setBounds(row.removeFromLeft(w));
    ra1.setBounds(row.removeFromLeft(w));
    ra2.setBounds(row.removeFromLeft(w));

    // Selected-node knob strip
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
    static std::vector<float> timeBuf; static std::vector<float> specDb;
    const int N = 1024; // small FFT for UI
    if ((int) timeBuf.size() < N) { timeBuf.assign(N, 0.0f); specDb.assign(N/2, -120.0f); }
    int read = proc.readAnalyzer(timeBuf.data(), N);
    if (read > 0)
    {
        juce::dsp::FFT fft((int) std::log2(N));
        std::vector<float> window(N, 0.0f);
        for (int i=0;i<N;++i) window[i] = 0.5f*(1.0f - std::cos(2.0f*juce::MathConstants<float>::pi*i/(N-1)));
        std::vector<float> fftBuf(2*N, 0.0f);
        for (int i=0;i<N;++i) fftBuf[i*2] = timeBuf[i] * window[i]; // real
        fft.performRealOnlyForwardTransform(fftBuf.data());
        for (int i=1;i<N/2;++i)
        {
            const float re = fftBuf[2*i]; const float im = fftBuf[2*i+1];
            const float mag = std::sqrt(re*re + im*im) / (float)N;
            specDb[(size_t)i] = juce::Decibels::gainToDecibels(std::max(mag, 1.0e-6f)) - 6.0f;
        }
        chart->setSpectrum(specDb, 0.0f, (float) (proc.getSampleRate() * 0.5));
    }

    // Crossovers + GR overlay
    std::vector<float> fcs; fcs.push_back(proc.apvts.getRawParameterValue("xover.1.Hz")->load());
    chart->setCrossovers(fcs);
    std::vector<float> gr = { proc.getBandGrDb(0), proc.getBandGrDb(1) };
    chart->setGRdB(gr);

    // Keep compressor chart and overlay synced with current band params
    const int b = juce::jlimit(1, 2, bandSel.getSelectedId() > 0 ? bandSel.getSelectedId() : 1);
    const auto id = [b](const char* name){ return juce::String("band.") + juce::String(b) + "." + name; };
    auto rp = [&](const juce::String& pid){ return proc.apvts.getRawParameterValue(pid)->load(); };
    if (compChart)
    {
        compChart->setThresholdDb(rp(id("threshold_dB")));
        compChart->setRatio(rp(id("ratio")));
        compChart->setKneeDb(rp(id("knee_dB")));
    }
    chart->setPrimaryBands(
        proc.apvts.getRawParameterValue("xover.1.Hz")->load(),
        proc.apvts.getRawParameterValue("band.1.threshold_dB")->load(),
        proc.apvts.getRawParameterValue("band.1.ratio")->load(),
        proc.apvts.getRawParameterValue("band.1.knee_dB")->load(),
        proc.apvts.getRawParameterValue("band.2.threshold_dB")->load(),
        proc.apvts.getRawParameterValue("band.2.ratio")->load(),
        proc.apvts.getRawParameterValue("band.2.knee_dB")->load());
}
