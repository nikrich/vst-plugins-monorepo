#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>
#include <cmath>

namespace CommonUI { namespace Charts {

// MultiBand Line Chart with optional Pro-Q-like overlay:
// - Live spectrum (dB vs frequency, log X)
// - Crossovers and shaded bands
// - GR markers
// - Optional interactive nodes (2 primary + decorative) controlling compressor params
class MBCLineChart : public juce::Component
{
public:
    enum class BandParam { Freq, Threshold, Ratio, Knee, Type };
    enum class CurveType { Bell, LowShelf, HighShelf, LowPass, HighPass, Notch };
    struct Style {
        juce::Colour bg { 0xFF0F1116 };
        juce::Colour grid { juce::Colours::white.withAlpha(0.12f) };
        juce::Colour spectrum { juce::Colour(0xFF66E1FF) }; // brighter cyan (pre)
        juce::Colour spectrumFill { juce::Colours::white.withAlpha(0.06f) }; // light grey underlay
        juce::Colour spectrumPost { 0xFFFFD45A }; // media yellow (post)
        juce::Colour gr { juce::Colour(0xFFFF7F7F) };
        juce::Colour crossover { juce::Colours::white.withAlpha(0.35f) };
        juce::Colour bandFillA { juce::Colours::purple.withAlpha(0.06f) };
        juce::Colour bandFillB { juce::Colours::blue.withAlpha(0.06f) };
        float specStroke = 1.6f;
        float gridStroke = 1.0f;
        float grRadius = 3.5f;

        // Overlay styling (used when primaries shown)
        juce::Colour overlayCurve{ 0xFF8E9EFF };
        juce::Colour overlayCurve2{ 0xFFFFA56B };
        juce::Colour overlayDecor{ juce::Colours::white.withAlpha(0.10f) };
        juce::Colour nodeFill{ juce::Colours::orange };
        juce::Colour nodeFill2{ juce::Colours::deeppink };
        juce::Colour combinedCurve{ 0xFFFFD45A }; // media yellow
        float nodeRadius = 6.0f;
        float combinedStroke = 2.4f;
        float otherCurveAlpha = 0.45f; // opacity for non-selected decorative curves
        float selectedFillAlpha = 0.18f; // translucent fill for selected band to 0 dB line
        float otherFillAlpha = 0.08f;    // translucent fill for non-selected bands
    };

    void setXRangeHz(float minHz, float maxHz) { xMinHz = juce::jmax(1.0f, minHz); xMaxHz = juce::jmax(xMinHz + 1.0f, maxHz); repaint(); }
    void setYRangeDb(float minDb, float maxDb) { yMinDb = minDb; yMaxDb = maxDb; repaint(); }

    void setCrossovers(const std::vector<float>& fc) { crossovers = fc; repaint(); }
    void setGRdB(const std::vector<float>& gr) { grDb = gr; repaint(); }

    // Provide spectrum in dB per bin mapped to Hz range [specMinHz, specMaxHz]
    void setSpectrum(const std::vector<float>& magsDb, float specMinHz, float specMaxHz)
    {
        // Temporal smoothing (lerp) to reduce erratic motion between frames
        if (prevSpectrumDb.size() != magsDb.size()) prevSpectrumDb = magsDb;
        for (size_t i = 0; i < magsDb.size(); ++i)
            prevSpectrumDb[i] = prevSpectrumDb[i] * spectrumTemporalBlend + magsDb[i] * (1.0f - spectrumTemporalBlend);
        spectrumDb = prevSpectrumDb;
        spMinHz = specMinHz; spMaxHz = specMaxHz; repaint();
    }
    void setPostSpectrum(const std::vector<float>& magsDb, float specMinHz, float specMaxHz)
    {
        if (prevPostSpectrumDb.size() != magsDb.size()) prevPostSpectrumDb = magsDb;
        for (size_t i = 0; i < magsDb.size(); ++i)
            prevPostSpectrumDb[i] = prevPostSpectrumDb[i] * spectrumTemporalBlend + magsDb[i] * (1.0f - spectrumTemporalBlend);
        postSpectrumDb = prevPostSpectrumDb;
        spMinHz = specMinHz; spMaxHz = specMaxHz; repaint();
    }

    // ===== Pro-Q-like overlay API =====
    void enableOverlay(bool b) { overlayEnabled = b; repaint(); }
    void setShowPrimaries(bool b) { showPrimaries = b; repaint(); }
    void enableGRMarkers(bool b) { showGR = b; repaint(); }
    void enableCombinedCurve(bool b) { showCombined = b; repaint(); }

    // Set the two primary bands (left/right) coming from compressor params
    void setPrimaryBands(float xoverHz,
                         float thLow, float raLow, float kneeLow,
                         float thHigh, float raHigh, float kneeHigh)
    {
        xover = juce::jlimit(xMinHz, xMaxHz, xoverHz);
        // Place left/right nodes around crossover with fixed multipliers for a good look
        bandL.freqHz = juce::jlimit(xMinHz, xMaxHz, xover * 0.65f);
        bandR.freqHz = juce::jlimit(xMinHz, xMaxHz, xover * 1.45f);
        bandL.thresholdDb = juce::jlimit(yMinDb, yMaxDb, thLow);
        bandR.thresholdDb = juce::jlimit(yMinDb, yMaxDb, thHigh);
        bandL.ratio = juce::jlimit(1.0f, 20.0f, raLow);
        bandR.ratio = juce::jlimit(1.0f, 20.0f, raHigh);
        bandL.kneeDb = juce::jlimit(0.0f, 24.0f, kneeLow);
        bandR.kneeDb = juce::jlimit(0.0f, 24.0f, kneeHigh);
        // Visual Q derived from knee
        bandL.q = kneeToQ(bandL.kneeDb);
        bandR.q = kneeToQ(bandR.kneeDb);
        // Do not auto-create decorative bands; start empty so user can add them explicitly.
        repaint();
    }

    // Callbacks to write back to host parameters
    std::function<void(float newXoverHz)> onChangeXover;
    std::function<void(int bandIndex, float newThresholdDb)> onChangeThreshold; // bandIndex: 0=low,1=high
    std::function<void(int bandIndex, float newRatio)> onChangeRatio;
    std::function<void(int bandIndex, float newKneeDb)> onChangeKnee;
    std::function<void(int newSelectedIndex)> onSelectionChanged;
    // Decorative band callback: index (0-based among decor), freq, gainDb, Q
    std::function<void(int index, float freqHz, float gainDb, float q)> onDecorChanged;

    struct SelectedInfo { float freqHz=0, thresholdDb=0, ratio=0, kneeDb=0; CurveType type=CurveType::Bell; bool isPrimary=true; int index=-1; };
    SelectedInfo getSelectedInfo() const
    {
        SelectedInfo si; si.index = selected;
        if (selected==0) { si.freqHz=bandL.freqHz; si.thresholdDb=bandL.thresholdDb; si.ratio=bandL.ratio; si.kneeDb=bandL.kneeDb; si.isPrimary=true; }
        else if (selected==1) { si.freqHz=bandR.freqHz; si.thresholdDb=bandR.thresholdDb; si.ratio=bandR.ratio; si.kneeDb=bandR.kneeDb; si.isPrimary=true; }
        else if (selected>=2 && (size_t)(selected-2) < decorBands.size()) {
            const auto& b = decorBands[(size_t)(selected-2)];
            si.freqHz=b.freqHz; si.thresholdDb=b.gainDb; si.ratio=b.ratio; si.kneeDb=b.kneeDb; si.isPrimary=false; si.type=CurveType::Bell;
        }
        return si;
    }
    void setSelectedBandValue(BandParam what, float v)
    {
        if (selected<0) return;
        if (selected==0)
        {
            switch (what){
                case BandParam::Freq: if (onChangeXover) onChangeXover(juce::jlimit(xMinHz, xMaxHz, v/0.65f)); bandL.freqHz=(float)v; break;
                case BandParam::Threshold: bandL.thresholdDb=(float) v; if (onChangeThreshold) onChangeThreshold(0, bandL.thresholdDb); break;
                case BandParam::Ratio: bandL.ratio=(float)v; if (onChangeRatio) onChangeRatio(0, bandL.ratio); break;
                case BandParam::Knee: bandL.kneeDb=(float)v; if (onChangeKnee) onChangeKnee(0, bandL.kneeDb); bandL.q=kneeToQ(bandL.kneeDb); break;
                case BandParam::Type: default: break; }
        }
        else if (selected==1)
        {
            switch (what){
                case BandParam::Freq: if (onChangeXover) onChangeXover(juce::jlimit(xMinHz, xMaxHz, v/1.45f)); bandR.freqHz=(float)v; break;
                case BandParam::Threshold: bandR.thresholdDb=(float) v; if (onChangeThreshold) onChangeThreshold(1, bandR.thresholdDb); break;
                case BandParam::Ratio: bandR.ratio=(float)v; if (onChangeRatio) onChangeRatio(1, bandR.ratio); break;
                case BandParam::Knee: bandR.kneeDb=(float)v; if (onChangeKnee) onChangeKnee(1, bandR.kneeDb); bandR.q=kneeToQ(bandR.kneeDb); break;
                case BandParam::Type: default: break; }
        }
        else if (selected>=2 && (size_t)(selected-2) < decorBands.size())
        {
            auto& b = decorBands[(size_t)(selected-2)];
            switch (what){
                case BandParam::Freq: b.freqHz = juce::jlimit(xMinHz, xMaxHz, (float)v); break;
                case BandParam::Threshold: b.gainDb = juce::jlimit(-24.0f, 24.0f, (float)v); break;
                case BandParam::Ratio: b.ratio = juce::jlimit(1.0f, 20.0f, (float)v); break;
                case BandParam::Knee: b.kneeDb = juce::jlimit(0.0f, 24.0f, (float)v); b.q = kneeToQ(b.kneeDb); break;
                case BandParam::Type: default: break; }
            if (onDecorChanged) onDecorChanged((int)(selected-2), b.freqHz, b.gainDb, b.q);
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.fillAll(style.bg);
        drawGrid(g, r);
        drawBands(g, r);
        drawSpectrum(g, r);
        drawCrossovers(g, r);
        if (showGR) drawGR(g, r);
        if (overlayEnabled) drawOverlay(g, r);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!overlayEnabled) return;
        auto r = getLocalBounds().toFloat();
        const auto p = e.getPosition().toFloat();
        const float rad = style.nodeRadius + 6.0f;

        // Check primary nodes (only if enabled)
        int newSel = -1;
        if (showPrimaries)
        {
            const auto l = juce::Point<float>(xToPx(bandL.freqHz, r), yToPx(bandL.thresholdDb, r));
            const auto rp = juce::Point<float>(xToPx(bandR.freqHz, r), yToPx(bandR.thresholdDb, r));
            if (p.getDistanceFrom(l) <= rad) newSel = 0;
            else if (p.getDistanceFrom(rp) <= rad) newSel = 1;
        }

        // Check decorative band nodes
        if (newSel < 0)
        {
            for (size_t i = 0; i < decorBands.size(); ++i)
            {
                const auto& b = decorBands[i];
                const auto bp = juce::Point<float>(xToPx(b.freqHz, r), yToPx(b.gainDb, r));
                if (p.getDistanceFrom(bp) <= rad)
                {
                    newSel = (int) i + 2; // offset after primaries
                    break;
                }
            }
        }

        selected = newSel;
        if (onSelectionChanged) onSelectionChanged(selected);
        dragStart = p;
        if (selected==0) startBand = bandL;
        else if (selected==1) startBand = bandR;
        else if (selected>=2 && (size_t)(selected-2) < decorBands.size()) startBand = decorBands[(size_t)(selected-2)];
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!overlayEnabled || selected < 0) return;
        auto r = getLocalBounds().toFloat();
        const auto p = e.getPosition().toFloat();

        // Convert current mouse pos to Hz / dB
        const float hzNow = pxToHz(p.x, r);
        const float dBNow = pxToDb(p.y, r);

        if ((selected == 0 || selected == 1) && showPrimaries)
        {
            // Primary nodes: horizontal drag moves crossover; vertical moves threshold; Alt adjusts ratio
            const float scale = (selected==0 ? 1.0f/0.65f : 1.0f/1.45f);
            float newXover = juce::jlimit(xMinHz*1.1f, xMaxHz*0.9f, hzNow * scale);
            if (onChangeXover) onChangeXover(newXover);

            if (e.mods.isAltDown())
            {
                const float tNorm = juce::jlimit(0.0f, 1.0f, (yMaxDb - dBNow) / (yMaxDb - yMinDb));
                float rVal = 1.0f + tNorm * 19.0f;
                if (onChangeRatio) onChangeRatio(selected, rVal);
            }
            else
            {
                float th = juce::jlimit(yMinDb, yMaxDb, dBNow);
                if (onChangeThreshold) onChangeThreshold(selected, th);
            }
        }
        else if (selected >= 2 && (size_t)(selected-2) < decorBands.size())
        {
            // Decorative bands: drag directly controls freq and threshold
            auto& b = decorBands[(size_t)(selected-2)];
            b.freqHz = juce::jlimit(xMinHz, xMaxHz, hzNow);
            // Vertical moves control boost/cut directly
            b.gainDb = juce::jlimit(-24.0f, 24.0f, dBNow);
            if (e.mods.isAltDown())
            {
                // Alt-drag vertically adjusts Q (faster with pixels)
                const float deltaPix = dragStart.y - p.y;
                b.q = juce::jlimit(0.01f, 64.0f, startBand.q + deltaPix * 0.01f);
            }
            if (onDecorChanged) onDecorChanged((int)(selected-2), b.freqHz, b.gainDb, b.q);
            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (!overlayEnabled || selected < 0) return;
        if (selected <= 1 && showPrimaries)
        {
            // Adjust knee (and visual Q) for primary bands
            float& knee = (selected==0? bandL.kneeDb : bandR.kneeDb);
            knee = juce::jlimit(0.0f, 24.0f, knee + (float)wheel.deltaY * 6.0f);
            if (onChangeKnee) onChangeKnee(selected, knee);
            if (selected==0) bandL.q = kneeToQ(bandL.kneeDb); else bandR.q = kneeToQ(bandR.kneeDb);
        }
        else if ((size_t)(selected-2) < decorBands.size())
        {
            // For decorative bands, wheel adjusts Q directly
            auto& b = decorBands[(size_t)(selected-2)];
            b.q = juce::jlimit(0.01f, 64.0f, b.q + (float)wheel.deltaY * 0.5f);
            if (onDecorChanged) onDecorChanged((int)(selected-2), b.freqHz, b.gainDb, b.q);
        }
        repaint();
    }

private:
    void setSpectrumTemporalBlend(float alpha) { spectrumTemporalBlend = juce::jlimit(0.0f, 0.99f, alpha); }

    float xToPx(float hz, juce::Rectangle<float> a) const
    {
        const float lx = std::log10(juce::jlimit(xMinHz, xMaxHz, hz));
        const float lmin = std::log10(xMinHz);
        const float lmax = std::log10(xMaxHz);
        return juce::jmap(lx, lmin, lmax, a.getX(), a.getRight());
    }
    float yToPx(float dB, juce::Rectangle<float> a) const
    {
        return juce::jmap(dB, yMaxDb, yMinDb, a.getY(), a.getBottom());
    }
    float pxToHz(float px, juce::Rectangle<float> a) const
    {
        const float lmin = std::log10(xMinHz);
        const float lmax = std::log10(xMaxHz);
        const float lx = juce::jmap(px, a.getX(), a.getRight(), lmin, lmax);
        return std::pow(10.0f, lx);
    }
    float pxToDb(float py, juce::Rectangle<float> a) const
    {
        return juce::jmap(py, a.getY(), a.getBottom(), yMaxDb, yMinDb);
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> a)
    {
        g.setColour(style.grid);
        g.drawRect(a);
        // Draw a few log-f verticals (decades from 20..20k)
        const float marks[] = {20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f};
        for (float f : marks) if (f >= xMinHz && f <= xMaxHz)
            g.drawLine(xToPx(f,a), a.getY(), xToPx(f,a), a.getBottom(), (f==1000.f?1.2f:0.6f));
        // Horizontal dB lines every 6 dB
        for (float y = std::ceil(yMinDb/6.f)*6.f; y <= yMaxDb; y+=6.f)
            g.drawLine(a.getX(), yToPx(y,a), a.getRight(), yToPx(y,a), (y==0.f?1.2f:0.6f));
    }

    void drawBands(juce::Graphics& g, juce::Rectangle<float> a)
    {
        // Shade alternating band regions
        std::vector<float> edges; edges.push_back(xMinHz); for (float fc: crossovers) edges.push_back(fc); edges.push_back(xMaxHz);
        for (size_t i=0;i+1<edges.size();++i)
        {
            auto x1 = xToPx(edges[i], a);
            auto x2 = xToPx(edges[i+1], a);
            g.setColour((i%2==0)? style.bandFillA : style.bandFillB);
            g.fillRect(juce::Rectangle<float>(x1, a.getY(), x2-x1, a.getHeight()));
        }
    }

    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> a)
    {
        if (spectrumDb.empty() && postSpectrumDb.empty()) return;

        // Higher-detail rendering: resample per pixel column with log-frequency mapping
        const int N = juce::jmax(2, (int) a.getWidth());
        juce::Path p; bool started=false;
        float prevYdB = 0.0f; bool havePrev=false;
        const int S = (int) spectrumDb.size();

        for (int i=0; i<N; ++i)
        {
            const float fracX = (float) i / (float) (N - 1);
            // Map screen X -> log frequency within chart range
            const float logHz = juce::jmap(fracX, 0.0f, 1.0f, std::log10(xMinHz), std::log10(xMaxHz));
            const float hz = std::pow(10.0f, logHz);

            // Map hz -> source spectrum fractional index (linear in hz across spMin..spMax)
            const float pos = juce::jlimit(0.0f, 1.0f, (hz - spMinHz) / juce::jmax(1.0f, (spMaxHz - spMinHz)));
            const float fidx = pos * (float) (S - 1);
            const int i0 = (int) std::floor(fidx);
            const int i1 = juce::jlimit(0, S-1, i0 + 1);
            const float t = fidx - (float) i0;
            const float ydBraw = (1.0f - t) * spectrumDb[(size_t) juce::jlimit(0,S-1,i0)] + t * spectrumDb[(size_t) i1];

            // Light smoothing for nicer look
            const float ydB = havePrev ? (0.85f * prevYdB + 0.15f * ydBraw) : ydBraw;
            prevYdB = ydB; havePrev = true;

            const float x = juce::jmap((float) i, 0.0f, (float) (N - 1), a.getX(), a.getRight());
            const float y = yToPx(ydB, a);

            if (!started) { p.startNewSubPath(x,y); started=true; }
            else p.lineTo(x,y);
        }

        if (started)
        {
            // Gradient fill under PRE spectrum (bottom -> near curve)
            juce::Path fillPath = p;
            fillPath.lineTo(a.getRight(), a.getBottom());
            fillPath.lineTo(a.getX(), a.getBottom());
            fillPath.closeSubPath();
            juce::ColourGradient grad(style.spectrumFill, a.getX(), a.getBottom(), style.spectrumFill.withAlpha(0.0f), a.getX(), a.getY(), false);
            g.setFillType(juce::FillType(grad));
            g.fillPath(fillPath);

            // Stroke PRE spectrum
            g.setColour(style.spectrum);
            g.strokePath(p, juce::PathStrokeType(style.specStroke));
        }

        // Draw POST spectrum (yellow) if available using same sampling
        if (!postSpectrumDb.empty())
        {
            juce::Path pp; bool stp=false;
            float prevYdB2 = 0.0f; bool havePrev2=false;
            const int S2 = (int) postSpectrumDb.size();
            for (int i=0; i<N; ++i)
            {
                const float fracX = (float) i / (float) (N - 1);
                const float logHz = juce::jmap(fracX, 0.0f, 1.0f, std::log10(xMinHz), std::log10(xMaxHz));
                const float hz = std::pow(10.0f, logHz);
                const float pos = juce::jlimit(0.0f, 1.0f, (hz - spMinHz) / juce::jmax(1.0f, (spMaxHz - spMinHz)));
                const float fidx = pos * (float) (S2 - 1);
                const int i0 = (int) std::floor(fidx);
                const int i1 = juce::jlimit(0, S2-1, i0 + 1);
                const float t = fidx - (float) i0;
                const float ydBraw = (1.0f - t) * postSpectrumDb[(size_t) juce::jlimit(0,S2-1,i0)] + t * postSpectrumDb[(size_t) i1];
                const float ydB = havePrev2 ? (0.85f * prevYdB2 + 0.15f * ydBraw) : ydBraw;
                prevYdB2 = ydB; havePrev2 = true;
                const float x = juce::jmap((float) i, 0.0f, (float) (N - 1), a.getX(), a.getRight());
                const float y = yToPx(ydB, a);
                if (!stp) { pp.startNewSubPath(x,y); stp=true; } else pp.lineTo(x,y);
            }
            g.setColour(style.spectrumPost.withAlpha(0.95f));
            g.strokePath(pp, juce::PathStrokeType(style.specStroke + 0.2f));
        }
    }

    void drawCrossovers(juce::Graphics& g, juce::Rectangle<float> a)
    {
        g.setColour(style.crossover);
        for (float fc : crossovers)
            g.drawLine(xToPx(fc,a), a.getY(), xToPx(fc,a), a.getBottom(), 1.0f);
    }

    void drawGR(juce::Graphics& g, juce::Rectangle<float> a)
    {
        if (grDb.empty()) return;
        // Plot markers at band centers
        std::vector<float> edges; edges.push_back(xMinHz); for (float fc: crossovers) edges.push_back(fc); edges.push_back(xMaxHz);
        g.setColour(style.gr);
        for (size_t i=0;i+1<edges.size() && i<grDb.size(); ++i)
        {
            const float fmid = std::sqrt(edges[i]*edges[i+1]); // log-mid
            const float x = xToPx(fmid, a);
            const float y = yToPx(-juce::jlimit(0.0f, 24.0f, grDb[i]), a); // show downward
            g.fillEllipse(juce::Rectangle<float>(style.grRadius*2, style.grRadius*2).withCentre({x,y}));
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        BandUI b; b.freqHz = pxToHz((float)e.x, getLocalBounds().toFloat()); b.gainDb=0.0f; b.q=1.0f;
        decorBands.push_back(b);
        selected = (int)decorBands.size() - 1 + 2; // select the new band
        if (onSelectionChanged) onSelectionChanged(selected);
        if (onDecorChanged) onDecorChanged((int)decorBands.size()-1, b.freqHz, b.gainDb, b.q);
        repaint();
    }

    // ===== Overlay drawing =====
    // For primaries: thresholdDb/ratio/kneeDb are compressor semantics.
    // For decorative EQ-like bands: gainDb is used for boost/cut and thresholdDb is ignored.
    struct BandUI { float freqHz=200.0f; float thresholdDb=-18.0f; float ratio=2.0f; float kneeDb=6.0f; float q=1.0f; float gainDb=0.0f; };

    static float kneeToQ(float knee) { return juce::jlimit(0.01f, 64.0f, 6.0f / juce::jmax(1.0f, knee + 0.5f)); }

    float bellGain(float hz, const BandUI& b) const
    {
        // Peaking curve purely for UI. 'gainDb' determines boost/cut (±24 dB by default).
        const float f0 = juce::jlimit(xMinHz, xMaxHz, b.freqHz);
        const float x = std::log(hz / f0 + 1.0e-12f);
        const float width = 1.0f / juce::jmax(0.01f, b.q); // allow much wider bells
        const float g = std::exp(-0.5f * (x/width) * (x/width));
        return b.gainDb * g;
    }

    void drawOverlay(juce::Graphics& g, juce::Rectangle<float> a)
    {
        // Decorative bands (each with its own vibrant colour)
        int idx = 0;
        for (auto& d : decorBands)
        {
            const float hue = std::fmod(0.58f + 0.12f * (float) idx, 1.0f);
            const auto col = juce::Colour::fromHSV(hue, 0.75f, 0.95f, 1.0f);
            const bool isSel = (selected == idx + 2);

            // Build curve path and optionally a fill to the 0 dB line
            juce::Path p; bool st=false; const int N=160;
            std::vector<juce::Point<float>> pts; pts.reserve(N);
            for (int i=0;i<N;++i)
            {
                const float frac = (float)i/(N-1);
                const float hz = std::pow(10.0f, juce::jmap(frac, 0.0f, 1.0f, std::log10(xMinHz), std::log10(xMaxHz)));
                const float yDb = bellGain(hz, d);
                const float x = xToPx(hz,a);
                const float y = yToPx(yDb, a);
                auto pt = juce::Point<float>(x,y);
                pts.push_back(pt);
                if (!st) { p.startNewSubPath(pt); st=true; } else p.lineTo(pt);
            }

            // Stroke with reduced opacity when not selected
            g.setColour(col.withAlpha(isSel ? 0.95f : style.otherCurveAlpha));
            g.strokePath(p, juce::PathStrokeType(2.0f));

            // Translucent fill to 0 dB (middle) line for all bands
            {
                const float y0 = yToPx(0.0f, a);
                juce::Path pf;
                pf.startNewSubPath(pts.front());
                for (size_t i=1;i<pts.size();++i) pf.lineTo(pts[i]);
                pf.lineTo(pts.back().withY(y0));
                pf.lineTo(pts.front().withY(y0));
                pf.closeSubPath();
                const float fillAlpha = isSel ? style.selectedFillAlpha : style.otherFillAlpha;
                g.setColour(col.withAlpha(fillAlpha));
                g.fillPath(pf);
            }

            // Node for decorative band (match curve colour)
            const float rnode = style.nodeRadius * 0.95f;
            auto centre = juce::Point<float>(xToPx(d.freqHz, a), yToPx(d.gainDb, a));
            auto nodeBounds = juce::Rectangle<float>(2*rnode, 2*rnode).withCentre(centre);
            if (isSel)
            {
                // Selected: white fill + white outer ring for clear focus
                g.setColour(juce::Colours::white);
                g.fillEllipse(nodeBounds);
                g.setColour(juce::Colours::white.withAlpha(0.95f));
                g.drawEllipse(nodeBounds.expanded(3.0f), 1.8f);
            }
            else
            {
                // Unselected: colored fill + subtle black outline
                g.setColour(col.withAlpha(0.95f));
                g.fillEllipse(nodeBounds);
                g.setColour(juce::Colours::black.withAlpha(0.75f));
                g.drawEllipse(nodeBounds, 1.0f);
            }
            ++idx;
        }

        // Primary left/right bands in bright colours
        auto drawPrimary = [&](const BandUI& b, juce::Colour col)
        {
            juce::Path p; bool st=false; const int N=160;
            for (int i=0;i<N;++i)
            {
                const float frac = (float)i/(N-1);
                const float hz = std::pow(10.0f, juce::jmap(frac, 0.0f, 1.0f, std::log10(xMinHz), std::log10(xMaxHz)));
                const float yDb = bellGain(hz, b);
                const float x = xToPx(hz,a);
                const float y = yToPx(yDb, a);
                if (!st) { p.startNewSubPath(x,y); st=true; } else p.lineTo(x,y);
            }
            g.setColour(col.withAlpha(0.85f));
            g.strokePath(p, juce::PathStrokeType(2.0f));
        };
        if (showPrimaries)
        {
            // Dim unselected primary curve if any primary is selected
            auto alphaL = (selected==0 ? 0.95f : style.otherCurveAlpha);
            auto alphaR = (selected==1 ? 0.95f : style.otherCurveAlpha);
            g.setColour(style.overlayCurve.withAlpha(alphaL));
            drawPrimary(bandL, style.overlayCurve.withAlpha(alphaL));
            g.setColour(style.overlayCurve2.withAlpha(alphaR));
            drawPrimary(bandR, style.overlayCurve2.withAlpha(alphaR));

            // Nodes
            const float r = style.nodeRadius;
            auto paintNode = [&](const BandUI& b, juce::Colour c)
            {
            auto centre = juce::Point<float>(xToPx(b.freqHz, a), yToPx(b.gainDb, a));
                g.setColour(c);
                g.fillEllipse(juce::Rectangle<float>(2*r, 2*r).withCentre(centre));
                g.setColour(juce::Colours::black.withAlpha(0.8f));
                g.drawEllipse(juce::Rectangle<float>(2*r, 2*r).withCentre(centre), 1.2f);
            };
            paintNode(bandL, style.nodeFill);
            paintNode(bandR, style.nodeFill2);

            // Selection highlight ring for primaries
            const float rsel = style.nodeRadius + 2.5f;
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            if (selected == 0)
                g.drawEllipse(juce::Rectangle<float>(2*rsel, 2*rsel).withCentre({xToPx(bandL.freqHz,a), yToPx(bandL.thresholdDb,a)}), 1.6f);
            else if (selected == 1)
                g.drawEllipse(juce::Rectangle<float>(2*rsel, 2*rsel).withCentre({xToPx(bandR.freqHz,a), yToPx(bandR.thresholdDb,a)}), 1.6f);
        }

        // Combined response (media yellow): sum of decorative band gains in dB
        if (showCombined)
        {
            juce::Path cp; bool stc=false; const int N=240;
            for (int i=0;i<N;++i)
            {
                const float frac = (float)i/(N-1);
                const float hz = std::pow(10.0f, juce::jmap(frac, 0.0f, 1.0f, std::log10(xMinHz), std::log10(xMaxHz)));
                float yDb = 0.0f;
                for (const auto& d : decorBands)
                    yDb += bellGain(hz, d); // sum dB contributions for visualisation
                const float x = xToPx(hz, a);
                const float y = yToPx(juce::jlimit(yMinDb, yMaxDb, yDb), a);
                if (!stc) { cp.startNewSubPath(x,y); stc=true; } else cp.lineTo(x,y);
            }
            g.setColour(style.combinedCurve.withAlpha(0.95f));
            g.strokePath(cp, juce::PathStrokeType(style.combinedStroke));
        }
    }

    void initDecorBands()
    {
        decorBands.clear();
        // 3 decorative bands spaced across spectrum
        BandUI a; a.freqHz = 90.0f;  a.thresholdDb = -6.0f;  a.q = 0.7f;
        BandUI b; b.freqHz = 900.0f; b.thresholdDb = -3.0f;  b.q = 1.2f;
        BandUI c; c.freqHz = 7000.0f;c.thresholdDb = -4.0f;  c.q = 0.9f;
        decorBands = { a,b,c };
    }

    Style style;
    float xMinHz = 20.0f, xMaxHz = 20000.0f;
    float yMinDb = -60.0f, yMaxDb = 12.0f;

    std::vector<float> crossovers; // size N-1 for N bands
    std::vector<float> grDb;       // size N bands
    std::vector<float> spectrumDb;     // pre-processed
    std::vector<float> postSpectrumDb; // post-processed
    std::vector<float> prevSpectrumDb;     // temporal smoothing state
    std::vector<float> prevPostSpectrumDb; // temporal smoothing state
    float spMinHz = 20.0f, spMaxHz = 20000.0f;

    // Temporal smoothing factor (0..0.99), higher = smoother/slower
    float spectrumTemporalBlend { 0.80f };

    // Overlay state
    bool overlayEnabled { false };
    bool showPrimaries { false }; // start with FabFilter-like empty state
    bool showGR { false };
    bool showCombined { true };
    float xover { 200.0f };
    BandUI bandL, bandR;
    std::vector<BandUI> decorBands;

    int selected { -1 };
    juce::Point<float> dragStart;
    BandUI startBand;
};

}} // namespace CommonUI::Charts
