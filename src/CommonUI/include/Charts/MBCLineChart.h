#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace CommonUI { namespace Charts {

// MultiBand Line Chart for top panel: draws
// - Live spectrum ("wave") path in dB vs frequency (log X)
// - Vertical lines for crossovers
// - Shaded band regions
// - Gain-Reduction markers per band
class MBCLineChart : public juce::Component
{
public:
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

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.fillAll(style.bg);
        drawGrid(g, r);
        drawBands(g, r);
        drawSpectrum(g, r);
        drawCrossovers(g, r);
        drawGR(g, r);
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

    Style style;
    float xMinHz = 20.0f, xMaxHz = 20000.0f;
    float yMinDb = -60.0f, yMaxDb = 12.0f;
    std::vector<float> crossovers; // size N-1 for N bands
    std::vector<float> grDb;       // size N bands

    std::vector<float> spectrumDb; float spMinHz = 20.0f, spMaxHz = 20000.0f;
};

}} // namespace CommonUI::Charts
