#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <Charts/MBCLineChart.h>

using CommonUI::Charts::MBCLineChart;

HungryGhostMultibandCompressorAudioProcessorEditor::HungryGhostMultibandCompressorAudioProcessorEditor(HungryGhostMultibandCompressorAudioProcessor& p)
: juce::AudioProcessorEditor(&p), proc(p)
{
    setOpaque(true);

    // Create and configure chart BEFORE calling setSize (which triggers resized())
    chart = std::make_unique<MBCLineChart>();
    chart->setXRangeHz(20.0f, 20000.0f);
    chart->setYRangeDb(-60.0f, 12.0f);

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

    addAndMakeVisible(*chart);

    // Defer setSize until after children are constructed
    setSize(600, 400);

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
    auto top = r.removeFromTop(240);
    if (chart)
        chart->setBounds(top);

    auto row = r.reduced(8);
    const int w = row.getWidth() / 4;
    th1.setBounds(row.removeFromLeft(w));
    th2.setBounds(row.removeFromLeft(w));
    ra1.setBounds(row.removeFromLeft(w));
    ra2.setBounds(row.removeFromLeft(w));
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
}
