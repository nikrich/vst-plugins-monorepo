#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace CommonUI { namespace Charts {

// A reusable FabFilter-like compressor static curve chart with draggable controls.
// Header-only component (CommonUI is header-only).
class CompressorChart : public juce::Component
{
public:
    struct Style {
        juce::Colour bg { 0xFF101015 };
        juce::Colour grid { juce::Colours::white.withAlpha(0.12f) };
        juce::Colour curve { juce::Colours::aqua }; // main curve
        juce::Colour diag { juce::Colours::white.withAlpha(0.18f) }; // y=x line
        juce::Colour handle { juce::Colours::orange };
        juce::Colour text { juce::Colours::white.withAlpha(0.7f) };
        float gridStroke = 1.0f;
        float curveStroke = 2.0f;
        float handleRadius = 6.0f;
    };

    CompressorChart()
    {
        setInterceptsMouseClicks(true, true);
    }

    // Parameters
    void setThresholdDb(float dB)  { thresholdDb = juce::jlimit(xMinDb, xMaxDb, dB); repaint(); }
    void setRatio(float r)         { ratio = juce::jmax(1.0f, r); repaint(); }
    void setKneeDb(float dB)       { kneeDb = juce::jmax(0.0f, dB); repaint(); }

    float getThresholdDb() const   { return thresholdDb; }
    float getRatio() const         { return ratio; }
    float getKneeDb() const        { return kneeDb; }

    // Axis ranges in dB (input X, output Y)
    void setRanges(float xMin, float xMax, float yMin, float yMax)
    {
        xMinDb = xMin; xMaxDb = xMax; yMinDb = yMin; yMaxDb = yMax; repaint();
    }

    // Callbacks (user drags)
    std::function<void(float newThresholdDb)> onThresholdChanged; // called continuously during drag
    std::function<void(float newRatio)>       onRatioChanged;     // ratio: >1
    std::function<void(float newKneeDb)>      onKneeChanged;      // >=0

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.fillAll(style.bg);
        drawGrid(g, r);
        drawDiagonal(g, r);
        drawCurve(g, r);
        drawHandles(g, r);
        drawLabels(g, r);
    }

    void resized() override {}

private:
    // Mapping
    juce::Point<float> toScreen(float xDb, float yDb, juce::Rectangle<float> area) const
    {
        const float px = juce::jmap(xDb, xMinDb, xMaxDb, area.getX(), area.getRight());
        const float py = juce::jmap(yDb, yMaxDb, yMinDb, area.getY(), area.getBottom());
        return { px, py };
    }

    float xFromScreen(float px, juce::Rectangle<float> area) const
    {
        return juce::jmap(px, area.getX(), area.getRight(), xMinDb, xMaxDb);
    }

    float yFromScreen(float py, juce::Rectangle<float> area) const
    {
        return juce::jmap(py, area.getY(), area.getBottom(), yMaxDb, yMinDb);
    }

    // Static compressor curve (downward): returns output level in dB for given input dB
    float compY(float xDb) const
    {
        const float T = thresholdDb;
        const float R = ratio;
        const float k = kneeDb;
        const float half = 0.5f * k;
        if (k <= 1.0e-6f)
        {
            if (xDb <= T) return xDb;
            return T + (xDb - T) / R;
        }
        if (xDb <= T - half) return xDb;
        if (xDb >= T + half) return T + (xDb - T) / R;
        // smooth knee
        const float y1 = xDb;
        const float y2 = T + (xDb - T) / R;
        const float t = (xDb - (T - half)) / (2.0f * half);
        return y1 + (y2 - y1) * (t * t * (3.0f - 2.0f * t));
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour(style.grid);
        g.drawRect(area, 1.0f);
        // simple ticks every 6 dB on X and Y
        for (float x = std::ceil(xMinDb / 6.0f) * 6.0f; x <= xMaxDb; x += 6.0f)
        {
            auto p1 = toScreen(x, yMinDb, area);
            auto p2 = toScreen(x, yMaxDb, area);
            g.drawLine({ p1.x, p1.y, p2.x, p2.y }, (x == 0.0f ? 1.2f : 0.6f));
        }
        for (float y = std::ceil(yMinDb / 6.0f) * 6.0f; y <= yMaxDb; y += 6.0f)
        {
            auto p1 = toScreen(xMinDb, y, area);
            auto p2 = toScreen(xMaxDb, y, area);
            g.drawLine({ p1.x, p1.y, p2.x, p2.y }, (y == 0.0f ? 1.2f : 0.6f));
        }
    }

    void drawDiagonal(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour(style.diag);
        auto p1 = toScreen(xMinDb, xMinDb, area);
        auto p2 = toScreen(xMaxDb, xMaxDb, area);
        g.drawLine({ p1.x, p1.y, p2.x, p2.y }, 1.0f);
    }

    void drawCurve(juce::Graphics& g, juce::Rectangle<float> area)
    {
        juce::Path path;
        const int N = juce::jmax(2, (int) area.getWidth());
        for (int i = 0; i < N; ++i)
        {
            const float xDb = juce::jmap((float) i, 0.0f, (float) (N - 1), xMinDb, xMaxDb);
            const float yDb = compY(xDb);
            const auto p = toScreen(xDb, yDb, area);
            if (i == 0) path.startNewSubPath(p); else path.lineTo(p);
        }
        g.setColour(style.curve);
        g.strokePath(path, juce::PathStrokeType(style.curveStroke));
    }

    void drawHandles(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Threshold handle at (T, compY(T))
        const auto thPos = toScreen(thresholdDb, compY(thresholdDb), area);
        g.setColour(style.handle);
        g.fillEllipse(juce::Rectangle<float>(style.handleRadius * 2.0f, style.handleRadius * 2.0f).withCentre(thPos));

        // Ratio handle at (T + knee/2 + offset, compY(...)) to provide a tangential control point
        const float rx = juce::jlimit(xMinDb, xMaxDb, thresholdDb + juce::jmax(3.0f, 0.5f * kneeDb + 3.0f));
        const auto rPos = toScreen(rx, compY(rx), area);
        g.fillEllipse(juce::Rectangle<float>(style.handleRadius * 2.0f, style.handleRadius * 2.0f).withCentre(rPos));

        // Knee width handle: placed above threshold point (vertical move adjusts knee)
        auto kPos = thPos.translated(0.0f, -30.0f);
        g.drawEllipse(juce::Rectangle<float>(style.handleRadius * 2.0f, style.handleRadius * 2.0f).withCentre(kPos), 1.5f);
    }

    void drawLabels(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour(style.text);
        g.setFont(12.0f);
        g.drawText(juce::String(thresholdDb, 1) + " dB", area.removeFromBottom(18.0f), juce::Justification::centred);
    }

    enum class Dragging { None, Threshold, Ratio, Knee };

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat();
        const auto thPos = toScreen(thresholdDb, compY(thresholdDb), r);
        const float rx = juce::jlimit(xMinDb, xMaxDb, thresholdDb + juce::jmax(3.0f, 0.5f * kneeDb + 3.0f));
        const auto rPos = toScreen(rx, compY(rx), r);
        const auto kPos = thPos.translated(0.0f, -30.0f);

        dragging = Dragging::None;
        if (e.getEventRelativeTo(this).position.getDistanceFrom(thPos) <= style.handleRadius + 4.0f)
            dragging = Dragging::Threshold;
        else if (e.getEventRelativeTo(this).position.getDistanceFrom(rPos) <= style.handleRadius + 4.0f)
            dragging = Dragging::Ratio;
        else if (e.getEventRelativeTo(this).position.getDistanceFrom(kPos) <= style.handleRadius + 6.0f)
            dragging = Dragging::Knee;
        dragStart = e.position;
        startThreshold = thresholdDb;
        startRatio = ratio;
        startKnee = kneeDb;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat();
        if (dragging == Dragging::Threshold)
        {
            const float xDbNew = xFromScreen(e.position.x, r);
            setThresholdDb(xDbNew);
            if (onThresholdChanged) onThresholdChanged(thresholdDb);
        }
        else if (dragging == Dragging::Ratio)
        {
            // Horizontal drag to change ratio: right increases ratio, left decreases toward 1
            const float delta = (e.position.x - dragStart.x) / juce::jmax(10.0f, r.getWidth() * 0.25f);
            const float rNew = juce::jlimit(1.0f, 20.0f, startRatio * std::pow(2.0f, delta * 3.0f));
            setRatio(rNew);
            if (onRatioChanged) onRatioChanged(ratio);
        }
        else if (dragging == Dragging::Knee)
        {
            // Vertical drag: up increases knee width, down decreases
            const float deltaPix = dragStart.y - e.position.y;
            const float kNew = juce::jlimit(0.0f, 24.0f, startKnee + deltaPix * 0.1f);
            setKneeDb(kNew);
            if (onKneeChanged) onKneeChanged(kneeDb);
        }
    }

    void mouseUp(const juce::MouseEvent&) override { dragging = Dragging::None; }

    Style style;
    float thresholdDb = -18.0f;
    float ratio = 2.0f;
    float kneeDb = 6.0f;
    float xMinDb = -60.0f, xMaxDb = 0.0f;
    float yMinDb = -60.0f, yMaxDb = 0.0f;

    Dragging dragging { Dragging::None };
    juce::Point<float> dragStart;
    float startThreshold = -18.0f;
    float startRatio = 2.0f;
    float startKnee = 6.0f;
};

}} // namespace CommonUI::Charts
