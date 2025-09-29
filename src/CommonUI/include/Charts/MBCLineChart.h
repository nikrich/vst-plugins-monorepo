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
        juce::Colour spectrum { juce::Colour(0xFF8AD1FF) };
        juce::Colour gr { juce::Colour(0xFFFF7F7F) };
        juce::Colour crossover { juce::Colours::white.withAlpha(0.35f) };
        juce::Colour bandFillA { juce::Colours::purple.withAlpha(0.08f) };
        juce::Colour bandFillB { juce::Colours::blue.withAlpha(0.08f) };
        float specStroke = 1.5f;
        float gridStroke = 1.0f;
        float grRadius = 3.5f;

        // Overlay styling
        juce::Colour overlayCurve{ 0xFF7C9EFF };
        juce::Colour overlayCurve2{ 0xFFFFB07C };
        juce::Colour overlayDecor{ juce::Colours::white.withAlpha(0.10f) };
        juce::Colour nodeFill{ juce::Colours::orange };
        juce::Colour nodeFill2{ juce::Colours::deeppink };
        float nodeRadius = 6.0f;
    };

    void setXRangeHz(float minHz, float maxHz) { xMinHz = juce::jmax(1.0f, minHz); xMaxHz = juce::jmax(xMinHz + 1.0f, maxHz); repaint(); }
    void setYRangeDb(float minDb, float maxDb) { yMinDb = minDb; yMaxDb = maxDb; repaint(); }

    void setCrossovers(const std::vector<float>& fc) { crossovers = fc; repaint(); }
    void setGRdB(const std::vector<float>& gr) { grDb = gr; repaint(); }

    // Provide spectrum in dB per bin mapped to Hz range [specMinHz, specMaxHz]
    void setSpectrum(const std::vector<float>& magsDb, float specMinHz, float specMaxHz)
    {
        spectrumDb = magsDb; spMinHz = specMinHz; spMaxHz = specMaxHz; repaint();
    }

    // ===== Pro-Q-like overlay API =====
    void enableOverlay(bool b) { overlayEnabled = b; repaint(); }

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
        if (decorBands.empty()) initDecorBands();
        repaint();
    }

    // Callbacks to write back to host parameters
    std::function<void(float newXoverHz)> onChangeXover;
    std::function<void(int bandIndex, float newThresholdDb)> onChangeThreshold; // bandIndex: 0=low,1=high
    std::function<void(int bandIndex, float newRatio)> onChangeRatio;
    std::function<void(int bandIndex, float newKneeDb)> onChangeKnee;
    std::function<void(int newSelectedIndex)> onSelectionChanged;

    struct SelectedInfo { float freqHz=0, thresholdDb=0, ratio=0, kneeDb=0; CurveType type=CurveType::Bell; bool isPrimary=true; int index=-1; };
    SelectedInfo getSelectedInfo() const
    {
        SelectedInfo si; si.index = selected;
        if (selected==0) { si.freqHz=bandL.freqHz; si.thresholdDb=bandL.thresholdDb; si.ratio=bandL.ratio; si.kneeDb=bandL.kneeDb; }
        else if (selected==1) { si.freqHz=bandR.freqHz; si.thresholdDb=bandR.thresholdDb; si.ratio=bandR.ratio; si.kneeDb=bandR.kneeDb; }
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
                case BandParam::Knee: bandL.kneeDb=(float)v; if (onChangeKnee) onChangeKnee(0, bandL.kneeDb); break;
                case BandParam::Type: default: break; }
        }
        else if (selected==1)
        {
            switch (what){
                case BandParam::Freq: if (onChangeXover) onChangeXover(juce::jlimit(xMinHz, xMaxHz, v/1.45f)); bandR.freqHz=(float)v; break;
                case BandParam::Threshold: bandR.thresholdDb=(float) v; if (onChangeThreshold) onChangeThreshold(1, bandR.thresholdDb); break;
                case BandParam::Ratio: bandR.ratio=(float)v; if (onChangeRatio) onChangeRatio(1, bandR.ratio); break;
                case BandParam::Knee: bandR.kneeDb=(float)v; if (onChangeKnee) onChangeKnee(1, bandR.kneeDb); break;
                case BandParam::Type: default: break; }
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
        drawGR(g, r);
        if (overlayEnabled) drawOverlay(g, r);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!overlayEnabled) return;
        auto r = getLocalBounds().toFloat();
        const auto p = e.getPosition().toFloat();
        const auto l = juce::Point<float>(xToPx(bandL.freqHz, r), yToPx(bandL.thresholdDb, r));
        const auto rP= juce::Point<float>(xToPx(bandR.freqHz, r), yToPx(bandR.thresholdDb, r));
        const float rad = style.nodeRadius + 4.0f;
        int newSel = selected;
        if (p.getDistanceFrom(l) <= rad) newSel = 0;
        else if (p.getDistanceFrom(rP) <= rad) newSel = 1;
        selected = newSel;
        if (onSelectionChanged) onSelectionChanged(selected);
        dragStart = p;
        startBand = (selected==0? bandL : bandR);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!overlayEnabled || selected < 0) return;
        auto r = getLocalBounds().toFloat();
        const auto p = e.getPosition().toFloat();
        const auto dx = p.x - dragStart.x;
        const auto dy = p.y - dragStart.y;

        // Convert current mouse pos to Hz / dB
        const float hzNow = pxToHz(p.x, r);
        const float dBNow = pxToDb(p.y, r);

        // Horizontal drag -> crossover (both nodes)
        const float scale = (selected==0 ? 1.0f/0.65f : 1.0f/1.45f);
        float newXover = hzNow * scale;
        newXover = juce::jlimit(xMinHz*1.1f, xMaxHz*0.9f, newXover);
        if (onChangeXover) onChangeXover(newXover);

        // Vertical drag -> threshold OR ratio with Alt/Option
        if (e.mods.isAltDown())
        {
            // map dB to ratio 1..20 (higher drag up -> larger ratio)
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

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (!overlayEnabled || selected < 0) return;
        // Adjust knee (and visual Q) with scroll
        float& knee = (selected==0? bandL.kneeDb : bandR.kneeDb);
        knee = juce::jlimit(0.0f, 24.0f, knee + (float)wheel.deltaY * 6.0f);
        if (onChangeKnee) onChangeKnee(selected, knee);
    }

private:
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
        if (spectrumDb.empty()) return;
        juce::Path p; bool started=false;
        const int N = (int) spectrumDb.size();
        for (int i=0;i<N;++i)
        {
            const float frac = (float) i / (float) juce::jmax(1,N-1);
            const float hz = spMinHz + frac * (spMaxHz - spMinHz);
            const float x = xToPx(hz, a);
            const float y = yToPx(spectrumDb[(size_t)i], a);
            if (!started) { p.startNewSubPath(x,y); started=true; }
            else p.lineTo(x,y);
        }
        g.setColour(style.spectrum);
        g.strokePath(p, juce::PathStrokeType(style.specStroke));
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
        BandUI b; b.freqHz = pxToHz((float)e.x, getLocalBounds().toFloat()); b.thresholdDb=-6.0f; b.q=1.0f;
        decorBands.push_back(b);
        repaint();
    }

    // ===== Overlay drawing =====
    struct BandUI { float freqHz=200.0f; float thresholdDb=-18.0f; float ratio=2.0f; float kneeDb=6.0f; float q=1.0f; };

    static float kneeToQ(float knee) { return juce::jlimit(0.2f, 8.0f, 6.0f / juce::jmax(1.0f, knee + 0.5f)); }

    float bellGain(float hz, const BandUI& b) const
    {
        // Simple peaking curve purely for UI: gain proportional to threshold and width by Q
        const float f0 = juce::jlimit(xMinHz, xMaxHz, b.freqHz);
        const float x = std::log(hz / f0 + 1.0e-12f);
        const float width = 1.0f / juce::jmax(0.2f, b.q);
        const float g = std::exp(-0.5f * (x/width) * (x/width));
        // Map threshold (negative) to downward dip magnitude visually
        const float mag = juce::jlimit(0.0f, 1.0f, (b.thresholdDb - yMinDb) / (yMaxDb - yMinDb));
        return -mag * g * 12.0f; // up to -12 dB dip for look only
    }

    void drawOverlay(juce::Graphics& g, juce::Rectangle<float> a)
    {
        // Decorative bands
        g.setColour(style.overlayDecor);
        for (auto& d : decorBands)
        {
            juce::Path p; bool st=false; const int N=140;
            for (int i=0;i<N;++i)
            {
                const float frac = (float)i/(N-1);
                const float hz = std::pow(10.0f, juce::jmap(frac, 0.0f, 1.0f, std::log10(xMinHz), std::log10(xMaxHz)));
                const float yDb = bellGain(hz, d);
                const float x = xToPx(hz,a);
                const float y = yToPx(yDb, a);
                if (!st) { p.startNewSubPath(x,y); st=true; } else p.lineTo(x,y);
            }
            g.strokePath(p, juce::PathStrokeType(1.5f));
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
        drawPrimary(bandL, style.overlayCurve);
        drawPrimary(bandR, style.overlayCurve2);

        // Nodes
        const float r = style.nodeRadius;
        auto paintNode = [&](const BandUI& b, juce::Colour c)
        {
            auto centre = juce::Point<float>(xToPx(b.freqHz, a), yToPx(b.thresholdDb, a));
            g.setColour(c);
            g.fillEllipse(juce::Rectangle<float>(2*r, 2*r).withCentre(centre));
            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.drawEllipse(juce::Rectangle<float>(2*r, 2*r).withCentre(centre), 1.2f);
        };
        paintNode(bandL, style.nodeFill);
        paintNode(bandR, style.nodeFill2);
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
    std::vector<float> spectrumDb; float spMinHz = 20.0f, spMaxHz = 20000.0f;

    // Overlay state
    bool overlayEnabled { false };
    float xover { 200.0f };
    BandUI bandL, bandR;
    std::vector<BandUI> decorBands;

    int selected { -1 };
    juce::Point<float> dragStart;
    BandUI startBand;
};

}} // namespace CommonUI::Charts
