#include "Horizon.h"

#include "Theme.h"

namespace augur5::ui
{

namespace
{
constexpr float fLo = 30.0f, fHi = 16000.0f;

const juce::Colour skyTop { 0xfffefdfb }, skyLow { 0xfff3eee6 }, groundTop { 0xfffbf9f5 }, groundLow { 0xfff2eee7 };
} // namespace

Horizon::Horizon (AudioTap& t, std::function<double()> sr) : tap (t), sampleRate (std::move (sr))
{
    setOpaque (true);
    setInterceptsMouseClicks (false, false);
    // Prime the history so the landscape is full from the first frame.
    std::array<float, static_cast<size_t> (points)> f {};
    for (int i = 0; i < layers; ++i)
    {
        time += 0.18f;
        makeIdleFrame (time, f.data());
        pushLayer (f.data(), 0.3f);
    }
    std::copy (f.begin(), f.end(), smoothed.begin());
    startTimerHz (30);
}

Horizon::~Horizon() { stopTimer(); }

void Horizon::makeIdleFrame (float t, float* out) const
{
    // A few slowly drifting formant hills and a fine ripple: a calm, breathing landscape.
    for (int i = 0; i < points; ++i)
    {
        const float x = static_cast<float> (i) / static_cast<float> (points - 1);
        const auto hill = [x] (float centre, float width, float height) { return height * std::exp (-(x - centre) * (x - centre) / width); };
        const float v = hill (0.20f + 0.05f * std::sin (t * 0.6f), 0.010f, 0.50f)
                      + hill (0.46f + 0.06f * std::sin (t * 0.41f + 1.0f), 0.018f, 0.38f)
                      + hill (0.72f + 0.04f * std::sin (t * 0.83f + 2.0f), 0.007f, 0.26f * (0.6f + 0.4f * std::sin (t * 1.2f)))
                      + 0.05f * (0.5f + 0.5f * std::sin (x * 41.0f + t * 1.9f)) * (1.0f - x);
        out[i] = juce::jlimit (0.0f, 1.0f, v * (0.25f + 0.75f * std::sin (juce::MathConstants<float>::pi * x)));
    }
}

void Horizon::pushLayer (const float* frame, float level)
{
    historyHead = (historyHead + 1) % layers;
    std::copy (frame, frame + points, history[static_cast<size_t> (historyHead)].begin());
    historyLevel[static_cast<size_t> (historyHead)] = level;
}

void Horizon::analyse()
{
    const int got = tap.pull (pullBuffer.data(), static_cast<int> (pullBuffer.size()));
    if (got > 0)
    {
        float sumSq = 0.0f;
        for (int i = 0; i < got; ++i)
        {
            const float v = pullBuffer[static_cast<size_t> (i)];
            ring[static_cast<size_t> (ringPos)] = v;
            ringPos = (ringPos + 1) % fftSize;
            sumSq += v * v;
        }
        rms = 0.8f * rms + 0.2f * std::sqrt (sumSq / static_cast<float> (got));
    }

    std::array<float, static_cast<size_t> (points)> frame {};
    if (got > 0)
    {
        for (int i = 0; i < fftSize; ++i)
            fftData[static_cast<size_t> (i)] = ring[static_cast<size_t> ((ringPos + i) % fftSize)];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), static_cast<size_t> (fftSize));
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        const double sr = sampleRate ? sampleRate() : 48000.0;
        const float binHz = static_cast<float> (sr > 0.0 ? sr : 48000.0) / static_cast<float> (fftSize);
        for (int i = 0; i < points; ++i)
        {
            const float x0 = static_cast<float> (i) / points, x1 = static_cast<float> (i + 1) / points;
            const int b0 = juce::jlimit (1, fftSize / 2 - 1, static_cast<int> (fLo * std::pow (fHi / fLo, x0) / binHz));
            const int b1 = juce::jlimit (b0 + 1, fftSize / 2, static_cast<int> (fLo * std::pow (fHi / fLo, x1) / binHz));
            float peak = 0.0f;
            for (int b = b0; b < b1; ++b)
                peak = juce::jmax (peak, fftData[static_cast<size_t> (b)]);
            const float db = juce::Decibels::gainToDecibels (peak / static_cast<float> (fftSize / 4), -100.0f);
            frame[static_cast<size_t> (i)] = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -72.0f, 0.0f, 0.0f, 1.0f));
        }
        const auto copy = frame;
        for (int i = 1; i < points - 1; ++i)
            frame[static_cast<size_t> (i)] = 0.25f * copy[static_cast<size_t> (i - 1)] + 0.5f * copy[static_cast<size_t> (i)]
                                           + 0.25f * copy[static_cast<size_t> (i + 1)];
    }

    // Idle landscape fades in after ~1 s of silence and out as soon as sound arrives.
    const bool silent = rms < 0.0005f || got == 0;
    silentFrames = silent ? silentFrames + 1 : 0;
    idleMix = juce::jlimit (0.0f, 1.0f, idleMix + (silentFrames > 30 ? 0.03f : -0.12f));
    if (got == 0)
        rms *= 0.92f;

    std::array<float, static_cast<size_t> (points)> idle {};
    time += 0.033f;
    makeIdleFrame (time, idle.data());

    for (int i = 0; i < points; ++i)
    {
        auto& s = smoothed[static_cast<size_t> (i)];
        const float target = juce::jmax (frame[static_cast<size_t> (i)], idle[static_cast<size_t> (i)] * idleMix * 0.8f);
        s += (target > s ? 0.55f : 0.12f) * (target - s);
    }
}

void Horizon::timerCallback()
{
    analyse();
    const float level = juce::jlimit (0.0f, 1.0f, rms * 5.0f);
    flap += 0.16f + 0.42f * level;
    if (++tick % 2 == 0) // a new ridge every other frame: the landscape flows into the distance
        pushLayer (smoothed.data(), juce::jlimit (0.0f, 1.0f, rms * 4.0f + idleMix * 0.3f));
    repaint();
}

void Horizon::paintBirds (juce::Graphics& g, juce::Rectangle<float> sky, float level) const
{
    // A loose V of nine birds crossing the sky, left to right, again and again.
    const float w = sky.getWidth();
    const float travel = std::fmod (time * 0.016f + 0.35f, 1.0f);
    const float cx = sky.getX() - w * 0.15f + travel * w * 1.3f;
    const float cy = sky.getY() + sky.getHeight() * (0.34f + 0.07f * std::sin (time * 0.23f)) - level * sky.getHeight() * 0.12f;
    for (int i = 0; i < 9; ++i)
    {
        const int k = (i + 1) / 2;
        const float side = i == 0 ? 0.0f : (i % 2 == 0 ? 1.0f : -1.0f);
        const float fi = static_cast<float> (i);
        const float x = cx - static_cast<float> (k) * 19.0f + 2.5f * std::sin (time * 0.7f + fi * 1.7f);
        const float y = cy + side * static_cast<float> (k) * 8.0f + 1.8f * std::sin (time * 0.9f + fi * 2.3f);
        const float s = 5.6f + 1.4f * std::sin (fi * 2.1f + 0.4f);
        const float beat = std::sin (flap + fi * 0.8f);
        const float tip = s * 0.55f * beat;

        juce::Path bird;
        bird.startNewSubPath (x - s, y + tip);
        bird.quadraticTo (x - s * 0.45f, y - s * 0.25f - tip * 0.3f, x, y + s * 0.12f);
        bird.quadraticTo (x + s * 0.45f, y - s * 0.25f - tip * 0.3f, x + s, y + tip);
        g.setColour (colours::ink.withAlpha (i == 0 ? 0.55f : 0.42f));
        g.strokePath (bird, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void Horizon::paintScale (juce::Graphics& g, juce::Rectangle<float> b) const
{
    // The frequency axis of the nearest ridge: fine ticks along the bottom edge.
    const float span = b.getWidth();
    const auto xOf = [&] (float f) { return b.getX() + span * std::log (f / fLo) / std::log (fHi / fLo); };
    g.setColour (colours::ink.withAlpha (0.16f));
    for (const float f : { 40.0f, 60.0f, 80.0f, 300.0f, 400.0f, 600.0f, 800.0f, 3000.0f, 4000.0f, 6000.0f, 8000.0f })
        g.fillRect (xOf (f) - 0.25f, b.getBottom() - 4.0f, 0.5f, 4.0f);
    const std::pair<float, const char*> marks[] { { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
                                                  { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" } };
    for (const auto& [f, text] : marks)
    {
        const float x = xOf (f);
        g.setColour (colours::ink.withAlpha (0.32f));
        g.fillRect (x - 0.4f, b.getBottom() - 7.0f, 0.8f, 7.0f);
        drawTracked (g, text, { x - 20.0f, b.getBottom() - 19.0f, 40.0f, 10.0f }, Fonts::mono (6.5f), colours::caption.withAlpha (0.9f),
                     juce::Justification::centred);
    }
    drawTracked (g, "Hz", { b.getRight() - 30.0f, b.getBottom() - 19.0f, 24.0f, 10.0f }, Fonts::mono (6.5f), colours::caption.withAlpha (0.9f),
                 juce::Justification::centredRight);
}

void Horizon::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();
    const float horizon = b.getY() + h * 0.47f;
    const float level = juce::jlimit (0.0f, 1.0f, rms * 5.0f);

    // Paper sky and ground.
    g.setGradientFill (juce::ColourGradient (skyTop, 0.0f, b.getY(), skyLow, 0.0f, horizon, false));
    g.fillRect (b.withBottom (horizon));
    g.setGradientFill (juce::ColourGradient (groundTop, 0.0f, horizon, groundLow, 0.0f, b.getBottom(), false));
    g.fillRect (b.withTop (horizon));

    // Pale sun resting on the far ridges: a soft halo, a few contour rings and the sunset cuts.
    const float sunR = h * (0.19f + 0.02f * level);
    const juce::Point<float> sun (b.getCentreX(), horizon - sunR * 0.42f);
    {
        const float haloR = sunR * 3.2f;
        juce::ColourGradient halo (colours::accent.withAlpha (0.10f + 0.06f * level), sun.x, sun.y, colours::accent.withAlpha (0.0f), sun.x + haloR, sun.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (sun.x - haloR, sun.y - haloR, haloR * 2.0f, haloR * 2.0f);
        g.setColour (colours::accent.withAlpha (0.07f));
        for (int i = 1; i <= 3; ++i)
        {
            const float rr = sunR * (1.0f + 0.32f * static_cast<float> (i));
            g.drawEllipse (sun.x - rr, sun.y - rr, rr * 2.0f, rr * 2.0f, 0.6f);
        }
        juce::ColourGradient disc (juce::Colour (0xfffbeee4), sun.x, sun.y - sunR * 0.6f, juce::Colour (0xfff0c4aa), sun.x, sun.y + sunR, false);
        g.setGradientFill (disc);
        g.fillEllipse (sun.x - sunR, sun.y - sunR, sunR * 2.0f, sunR * 2.0f);
        // Cuts across the lower half, thicker towards the horizon.
        for (int i = 0; i < 5; ++i)
        {
            const float fi = static_cast<float> (i);
            const float y = sun.y + sunR * (0.12f + 0.18f * fi);
            const float thick = 0.8f + 0.7f * fi;
            g.setColour (skyLow.interpolatedWith (skyTop, 0.3f));
            g.fillRect (sun.x - sunR, y, sunR * 2.0f, thick);
        }
    }

    paintBirds (g, b.withBottom (horizon), level);

    // Reflection: fine dashes under the horizon.
    for (int i = 0; i < 10; ++i)
    {
        const float fi = static_cast<float> (i);
        const float yy = horizon + 4.0f + fi * h * 0.032f;
        const float ww = sunR * (1.2f - fi * 0.09f) * (0.75f + 0.25f * std::sin (time * 2.0f + fi * 1.3f));
        g.setColour (colours::accent.withAlpha (0.16f * (1.0f - fi / 10.0f)));
        g.fillRoundedRectangle (sun.x - ww * 0.5f, yy, ww, 1.2f, 0.6f);
    }

    // Horizon line.
    {
        juce::ColourGradient line (colours::ink.withAlpha (0.0f), b.getX(), horizon, colours::ink.withAlpha (0.0f), b.getRight(), horizon, false);
        line.addColour (0.5, colours::ink.withAlpha (0.28f));
        g.setGradientFill (line);
        g.fillRect (b.getX(), horizon, w, 0.8f);
    }

    // Ridges, oldest (far, faint, narrow) to newest (near, inked, full width).
    for (int k = 0; k < layers; ++k)
    {
        const int idx = (historyHead + 1 + k) % layers;
        const float t = static_cast<float> (k) / static_cast<float> (layers - 1);
        const auto& data = history[static_cast<size_t> (idx)];
        const float depth = std::pow (t, 1.6f);
        const float baseY = horizon + 5.0f + depth * (h * 0.42f);
        const float amp = h * (0.09f + 0.34f * depth);
        const float xScale = 0.6f + 0.4f * t;
        const float x0 = b.getCentreX() - w * 0.5f * xScale;
        const float span = w * xScale;

        juce::Path ridge;
        const auto pt = [&] (int i) {
            return juce::Point<float> (x0 + span * static_cast<float> (i) / static_cast<float> (points - 1), baseY - amp * data[static_cast<size_t> (i)]);
        };
        ridge.startNewSubPath (pt (0));
        for (int i = 1; i < points - 1; ++i)
        {
            const auto p = pt (i), q = pt (i + 1);
            ridge.quadraticTo (p, (p + q) * 0.5f);
        }
        ridge.lineTo (pt (points - 1));

        juce::Path fill (ridge);
        fill.lineTo (x0 + span, b.getBottom());
        fill.lineTo (x0, b.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.97f), 0.0f, baseY - amp, groundLow, 0.0f, b.getBottom(), false));
        g.fillPath (fill);

        const float lvl = historyLevel[static_cast<size_t> (idx)];
        if (k == layers - 1)
        {
            g.setColour (colours::accent.withAlpha (0.18f * (0.4f + lvl)));
            g.strokePath (ridge, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colours::accent.withAlpha (0.92f));
            g.strokePath (ridge, juce::PathStrokeType (1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour (colours::ink.withAlpha (0.08f + 0.55f * t * t + 0.1f * lvl * t));
            g.strokePath (ridge, juce::PathStrokeType (0.55f + 0.55f * t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    paintScale (g, b.reduced (4.0f, 2.0f));

    // Soft side fades into the white strip, and a hairline frame.
    const auto edge = colours::panelTop;
    g.setGradientFill (juce::ColourGradient (edge, b.getX(), 0.0f, edge.withAlpha (0.0f), b.getX() + w * 0.1f, 0.0f, false));
    g.fillRect (b.withWidth (w * 0.1f));
    g.setGradientFill (juce::ColourGradient (edge.withAlpha (0.0f), b.getRight() - w * 0.1f, 0.0f, edge, b.getRight(), 0.0f, false));
    g.fillRect (b.withLeft (b.getRight() - w * 0.1f));
    g.setColour (colours::panelBorder);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace augur5::ui
