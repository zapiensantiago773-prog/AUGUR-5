#include "Displays.h"

#include "../ParameterLayout.h"
#include "../Parameters.h"
#include "Engine/SynthParams.h"

#include <complex>

namespace augur5::ui
{

namespace
{
namespace P = augur5::params;

void paintPlotBackground (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (colours::subPanel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::subPanelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

void strokeSoft (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
{
    g.setColour (c.withAlpha (0.15f));
    g.strokePath (p, juce::PathStrokeType (width + 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

//==============================================================================
ParamView::ParamView (APVTS& state, std::initializer_list<juce::String> ids)
{
    for (const auto& id : ids)
        values.push_back (state.getRawParameterValue (id));
    last.assign (values.size(), -12345.0f);
    setInterceptsMouseClicks (false, false);
}

void ParamView::poll()
{
    bool changed = false;
    for (size_t i = 0; i < values.size(); ++i)
    {
        const float v = value (i);
        if (v != last[i])
        {
            last[i] = v;
            changed = true;
        }
    }
    if (changed)
        repaint();
}

//==============================================================================
FilterCurve::FilterCurve (APVTS& state, bool hpf)
    : ParamView (state, { P::flt_cutoff, P::flt_reso, P::flt_model, P::flt_slope, P::flt_mode, P::hpf_cutoff }), withHpf (hpf)
{
}

void FilterCurve::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    paintPlotBackground (g, r);
    const auto plot = r.reduced (10.0f, 8.0f).withTrimmedBottom (10.0f);
    g.setColour (colours::hairline);
    for (const float f : { 100.0f, 1000.0f, 10000.0f })
        g.fillRect (plot.getX() + plot.getWidth() * std::log10 (f / 20.0f) / 3.0f, plot.getY(), 1.0f, plot.getHeight());

    const float fc = juce::jmax (20.0f, value (0)), reso = juce::jlimit (0.0f, 1.0f, value (1));
    const int model = juce::roundToInt (value (2)), slope = juce::roundToInt (value (3)), mode = juce::roundToInt (value (4));
    const float hpf = value (5);
    const bool multimode = model == 3;
    constexpr float lo = -42.0f, hi = 18.0f;

    const auto response = [&] (float f) {
        const std::complex<float> s (0.0f, f / fc);
        std::complex<float> h;
        if (! multimode || (slope == 0 && mode == 0))
        {
            // Four-pole ladder: (1 + s)^4 with the resonance loop; the passband drops as the loop gain rises.
            const float k = 3.9f * reso;
            const auto stage = 1.0f + s;
            h = (1.0f + 0.45f * k) / (stage * stage * stage * stage + k);
        }
        else
        {
            const float q = 0.55f + 9.0f * reso * reso;
            const auto den = s * s + s / q + 1.0f;
            h = mode == 0 ? 1.0f / den : (mode == 1 ? (s / q) / den : (s * s) / den);
            if (slope == 0)
                h = h * h;
        }
        if (withHpf && hpf > 10.5f)
        {
            const std::complex<float> sh (0.0f, f / hpf);
            h *= (sh * sh) / (sh * sh + 1.414f * sh + 1.0f);
        }
        return juce::Decibels::gainToDecibels (std::abs (h), -80.0f);
    };

    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float x = static_cast<float> (i) / 200.0f;
        const float db = juce::jlimit (lo, hi, response (20.0f * std::pow (1000.0f, x)));
        const juce::Point<float> pt (plot.getX() + x * plot.getWidth(), plot.getBottom() - (db - lo) / (hi - lo) * plot.getHeight());
        i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
    }
    juce::Path area (p);
    area.lineTo (plot.getRight(), plot.getBottom());
    area.lineTo (plot.getX(), plot.getBottom());
    area.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.13f), 0.0f, plot.getY(), colours::accent.withAlpha (0.0f), 0.0f,
                                             plot.getBottom(), false));
    g.fillPath (area);
    // 0 dB reference.
    g.setColour (colours::tickMinor);
    g.fillRect (plot.getX(), plot.getBottom() - (0.0f - lo) / (hi - lo) * plot.getHeight(), plot.getWidth(), 0.6f);
    strokeSoft (g, p, colours::accent, 1.4f);

    // Cutoff marker.
    const float xc = plot.getX() + plot.getWidth() * juce::jlimit (0.0f, 1.0f, std::log10 (fc / 20.0f) / 3.0f);
    g.setColour (colours::ink.withAlpha (0.35f));
    g.fillRect (xc - 0.4f, plot.getY(), 0.8f, plot.getHeight());
    const auto text = fc >= 1000.0f ? juce::String (fc / 1000.0f, fc >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (fc)) + " Hz";
    drawTracked (g, text, { juce::jlimit (plot.getX(), plot.getRight() - 64.0f, xc + 4.0f), plot.getY(), 64.0f, 11.0f }, Fonts::mono (7.5f),
                 colours::label, juce::Justification::centredLeft);
    for (const auto& [f, label] : { std::pair { 100.0f, "100" }, std::pair { 1000.0f, "1k" }, std::pair { 10000.0f, "10k" } })
    {
        const float x = plot.getX() + plot.getWidth() * std::log10 (f / 20.0f) / 3.0f;
        drawTracked (g, label, { x - 16.0f, plot.getBottom() + 1.0f, 32.0f, 10.0f }, Fonts::mono (6.5f), colours::caption, juce::Justification::centred);
    }
}

//==============================================================================
LfoShape::LfoShape (APVTS& state) : ParamView (state, { P::lfo2_rate, P::lfo2_wave, P::lfo2_sync }) {}

void LfoShape::poll()
{
    const double rate = value (2) > 0.5f ? 1.0 : juce::jlimit (0.05, 30.0, static_cast<double> (value (0)));
    phase = std::fmod (phase + juce::jmin (rate, 6.0) / 30.0, 2.0);
    repaint();
}

void LfoShape::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    paintPlotBackground (g, r);
    const auto plot = r.reduced (14.0f, 14.0f);
    g.setColour (colours::hairline);
    g.fillRect (plot.getX(), plot.getCentreY(), plot.getWidth(), 1.0f);
    const int wave = juce::roundToInt (value (1));

    const auto rnd = [] (int n) {
        auto x = static_cast<juce::uint32> (n * 2654435761u + 0x9e3779b9u);
        x ^= x >> 15;
        x *= 0x2c1b3c6du;
        x ^= x >> 12;
        return static_cast<float> (x & 0xffff) / 32767.5f - 1.0f;
    };
    const auto shape = [&] (double ph) -> float {
        const double f = ph - std::floor (ph);
        const int cycle = static_cast<int> (std::floor (ph * 4.0));
        switch (wave)
        {
            case 0: return static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * f));
            case 1: return static_cast<float> (f < 0.25 ? 4.0 * f : (f < 0.75 ? 2.0 - 4.0 * f : 4.0 * f - 4.0));
            case 2: return static_cast<float> (2.0 * f - 1.0);
            case 3: return static_cast<float> (1.0 - 2.0 * f);
            case 4: return f < 0.5 ? 1.0f : -1.0f;
            case 5: return rnd (cycle);
            default:
            {
                const double u = ph * 4.0 - std::floor (ph * 4.0);
                const float a = rnd (cycle), b = rnd (cycle + 1);
                const float w = static_cast<float> (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * u));
                return a + (b - a) * w;
            }
        }
    };

    juce::Path p;
    constexpr int n = 260;
    for (int i = 0; i <= n; ++i)
    {
        const double ph = 2.0 * i / n;
        const float y = plot.getCentreY() - 0.42f * plot.getHeight() * shape (ph);
        const float x = plot.getX() + plot.getWidth() * static_cast<float> (i) / n;
        i == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
    }
    strokeSoft (g, p, colours::slate, 1.4f);
    const float dx = plot.getX() + plot.getWidth() * static_cast<float> (phase / 2.0);
    const float dy = plot.getCentreY() - 0.42f * plot.getHeight() * shape (phase);
    g.setColour (colours::slate.withAlpha (0.18f));
    g.fillEllipse (dx - 8.0f, dy - 8.0f, 16.0f, 16.0f);
    g.setColour (colours::slate);
    g.fillEllipse (dx - 3.5f, dy - 3.5f, 7.0f, 7.0f);
}

//==============================================================================
MatrixMap::MatrixMap (APVTS& state)
    : ParamView (state, { P::mmSrc (1), P::mmDst (1), P::mmAmt (1), P::mmSrc (2), P::mmDst (2), P::mmAmt (2), P::mmSrc (3), P::mmDst (3),
                          P::mmAmt (3), P::mmSrc (4), P::mmDst (4), P::mmAmt (4), P::mmSrc (5), P::mmDst (5), P::mmAmt (5), P::mmSrc (6),
                          P::mmDst (6), P::mmAmt (6), P::mmSrc (7), P::mmDst (7), P::mmAmt (7), P::mmSrc (8), P::mmDst (8), P::mmAmt (8),
                          P::lfo_amount })
{
}

void MatrixMap::paint (juce::Graphics& g)
{
    // The top band (level with the section title) carries the notes; the map is drawn below it.
    const auto notesBand = getLocalBounds().toFloat().withHeight (16.0f);
    const auto r = getLocalBounds().toFloat().withTrimmedTop (36.0f);
    const auto& sources = matrixSourceNames();
    const auto& dests = matrixDestNames();
    const float topY = r.getY() + 16.0f, bottomY = r.getBottom() - 16.0f;
    const float srcW = r.getWidth() / static_cast<float> (sources.size());
    const float dstW = r.getWidth() / static_cast<float> (dests.size());
    const auto srcX = [&] (int i) { return r.getX() + srcW * (static_cast<float> (i) + 0.5f); };
    const auto dstX = [&] (int i) { return r.getX() + dstW * (static_cast<float> (i) + 0.5f); };

    std::vector<bool> srcUsed (static_cast<size_t> (sources.size())), dstUsed (static_cast<size_t> (dests.size()));
    struct Route
    {
        int slot, src, dst;
        float amt;
        bool active;
        int twin; // how many earlier routes share this source and destination (their badges are spread out)
    };
    // A route does nothing when the engine cannot apply it (per-voice source -> the shared LFO's rate), or when its
    // source is the LFO with LFO AMOUNT at 0 (the amount scales every use of the LFO).
    const bool lfoSilent = value (24) < 0.001f;
    bool anyRateNote = false, anyLfoNote = false;
    std::vector<Route> routes;
    for (int s = 0; s < 8; ++s)
    {
        const int src = juce::jlimit (0, sources.size() - 1, juce::roundToInt (value (static_cast<size_t> (s * 3))));
        const int dst = juce::jlimit (0, dests.size() - 1, juce::roundToInt (value (static_cast<size_t> (s * 3 + 1))));
        const float amt = value (static_cast<size_t> (s * 3 + 2));
        if (std::abs (amt) < 0.005f)
            continue;
        const bool engineOk = augur::isMatrixRouteActive (static_cast<augur::ModSource> (src), static_cast<augur::ModDest> (dst));
        const bool lfoOk = ! (lfoSilent && static_cast<augur::ModSource> (src) == augur::ModSource::Lfo);
        anyRateNote = anyRateNote || ! engineOk;
        anyLfoNote = anyLfoNote || ! lfoOk;
        int twin = 0;
        for (const auto& earlier : routes)
            twin += earlier.src == src && earlier.dst == dst ? 1 : 0;
        routes.push_back ({ s + 1, src, dst, amt, engineOk && lfoOk, twin });
        if (engineOk && lfoOk)
        {
            srcUsed[static_cast<size_t> (src)] = true;
            dstUsed[static_cast<size_t> (dst)] = true;
        }
    }

    // Rails.
    g.setColour (colours::hairline);
    g.fillRect (r.getX(), topY + 9.0f, r.getWidth(), 1.0f);
    g.fillRect (r.getX(), bottomY - 10.0f, r.getWidth(), 1.0f);
    for (int i = 0; i < sources.size(); ++i)
    {
        const bool used = srcUsed[static_cast<size_t> (i)];
        drawTracked (g, sources[i], { srcX (i) - srcW * 0.5f + 2.0f, r.getY(), srcW - 4.0f, 12.0f }, Fonts::jost (8.0f, used, 0.1f),
                     used ? colours::title : colours::caption, juce::Justification::centred);
        drawLedDot (g, { srcX (i) - 3.0f, topY + 6.5f, 6.0f, 6.0f }, colours::accent, used ? 1.0f : 0.0f);
    }
    for (int i = 0; i < dests.size(); ++i)
    {
        const bool used = dstUsed[static_cast<size_t> (i)];
        drawTracked (g, dests[i], { dstX (i) - dstW * 0.5f + 2.0f, r.getBottom() - 12.0f, dstW - 4.0f, 12.0f }, Fonts::jost (8.0f, used, 0.1f),
                     used ? colours::title : colours::caption, juce::Justification::centred);
        drawLedDot (g, { dstX (i) - 3.0f, bottomY - 13.0f, 6.0f, 6.0f }, colours::slate, used ? 1.0f : 0.0f);
    }

    // Inactive routes first (underneath), dashed and grey; then the working ones, thickness = amount.
    for (const bool drawActive : { false, true })
        for (const auto& rt : routes)
        {
            if (rt.active != drawActive)
                continue;
            const juce::Point<float> a (srcX (rt.src), topY + 13.0f), b (dstX (rt.dst), bottomY - 14.0f);
            const float midY = (a.y + b.y) * 0.5f;
            juce::Path p;
            p.startNewSubPath (a);
            p.cubicTo (a.x, midY, b.x, midY, b.x, b.y);
            const float mag = std::abs (rt.amt);
            const auto c = ! rt.active ? colours::caption : (rt.amt > 0.0f ? colours::accent : colours::slate);
            if (rt.active)
            {
                g.setColour (c.withAlpha (0.12f));
                g.strokePath (p, juce::PathStrokeType (3.0f + 6.0f * mag, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (c.withAlpha (0.45f + 0.5f * mag));
                g.strokePath (p, juce::PathStrokeType (0.8f + 1.6f * mag, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
            {
                juce::Path dashed;
                const float dashes[] { 4.0f, 4.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dashes, 2);
                g.setColour (c.withAlpha (0.8f));
                g.fillPath (dashed);
            }
            // Badges of routes sharing a source and destination sit apart along the line.
            const float along = 0.5f + (rt.twin % 2 == 1 ? -1.0f : 1.0f) * 0.13f * static_cast<float> ((rt.twin + 1) / 2);
            const auto mid = p.getPointAlongPath (p.getLength() * juce::jlimit (0.15f, 0.85f, along));
            g.setColour (juce::Colours::white);
            g.fillEllipse (mid.x - 8.0f, mid.y - 8.0f, 16.0f, 16.0f);
            g.setColour (c);
            g.drawEllipse (mid.x - 8.0f, mid.y - 8.0f, 16.0f, 16.0f, 1.0f);
            drawTracked (g, juce::String (rt.slot), { mid.x - 8.0f, mid.y - 7.0f, 16.0f, 14.0f }, Fonts::mono (7.5f), c.darker (0.2f),
                         juce::Justification::centred);
        }

    if (routes.empty())
        drawTracked (g, "NO ACTIVE ROUTES  -  SET AN AMOUNT IN THE MATRIX ABOVE", r.withSizeKeepingCentre (r.getWidth(), 16.0f),
                     Fonts::jost (9.0f, true, 0.2f), colours::caption, juce::Justification::centred);

    // Why a dashed route does nothing, level with the section title.
    juce::StringArray notes;
    if (anyRateNote)
        notes.add ("LFO RATE FOLLOWS ONLY MOD WHEEL AND AFTERTOUCH (ONE LFO FOR ALL VOICES)");
    if (anyLfoNote)
        notes.add ("LFO AMOUNT IS AT 0 (MAIN > LFO)");
    if (! notes.isEmpty())
    {
        const auto band = notesBand.withTrimmedLeft (160.0f);
        const auto text = "DASHED = NO EFFECT:  " + notes.joinIntoString ("  /  ");
        drawTracked (g, text, band, Fonts::jost (8.5f, true, 0.12f), colours::captionLight, juce::Justification::centredRight);
        const float w = juce::jmin (band.getWidth(), juce::GlyphArrangement::getStringWidth (Fonts::jost (8.5f, true, 0.12f), text));
        // A dashed sample before the note.
        juce::Path sample, dashed;
        sample.startNewSubPath (band.getRight() - w - 34.0f, band.getCentreY());
        sample.lineTo (band.getRight() - w - 10.0f, band.getCentreY());
        const float dashes[] { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, sample, dashes, 2);
        g.setColour (colours::caption);
        g.fillPath (dashed);
    }
}

//==============================================================================
ArpPattern::ArpPattern (APVTS& state, std::function<double()> t)
    : ParamView (state, { P::arp_on, P::arp_mode, P::arp_oct, P::arp_rate, P::arp_gate, P::arp_swing }), tempo (std::move (t))
{
}

void ArpPattern::poll()
{
    ParamView::poll();
    if (value (0) < 0.5f)
    {
        if (shownStep != -1)
        {
            shownStep = -1;
            repaint();
        }
        return;
    }
    const double bpm = tempo ? juce::jlimit (20.0, 400.0, tempo()) : 120.0;
    beat += bpm / 60.0 / 30.0;
    const double stepBeats = augur::arpRateBeats[static_cast<size_t> (juce::jlimit (0, 7, juce::roundToInt (value (3))))];
    const int step = static_cast<int> (std::floor (beat / stepBeats)) % 16;
    if (step != shownStep)
    {
        shownStep = step;
        repaint();
    }
}

void ArpPattern::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    paintPlotBackground (g, r);
    const auto plot = r.reduced (18.0f, 18.0f);
    const int mode = juce::roundToInt (value (1));
    const int octaves = juce::jlimit (1, 4, juce::roundToInt (value (2)));
    const float gate = juce::jlimit (0.02f, 1.0f, value (4)), swing = juce::jlimit (0.0f, 0.5f, value (5));

    // The pattern over C - E - G (ORDER plays them as held: C, G, E).
    std::vector<int> up;
    for (int o = 0; o < octaves; ++o)
        for (const int n : { 0, 4, 7 })
            up.push_back (n + 12 * o);
    std::vector<int> seq;
    switch (mode)
    {
        case 0: seq = up; break;
        case 1: seq.assign (up.rbegin(), up.rend()); break;
        case 2:
            seq = up;
            for (int i = static_cast<int> (up.size()) - 2; i >= 1; --i)
                seq.push_back (up[static_cast<size_t> (i)]);
            break;
        case 3:
        {
            juce::Random rng (7);
            for (int i = 0; i < 16; ++i)
                seq.push_back (up[static_cast<size_t> (rng.nextInt (static_cast<int> (up.size())))]);
            break;
        }
        default:
            for (int o = 0; o < octaves; ++o)
                for (const int n : { 0, 7, 4 })
                    seq.push_back (n + 12 * o);
            break;
    }
    const int top = 12 * (octaves - 1) + 7;
    const float colW = plot.getWidth() / 16.0f;
    for (int i = 0; i <= 16; ++i)
    {
        g.setColour (i % 4 == 0 ? colours::tickMinor : colours::hairline);
        g.fillRect (plot.getX() + colW * static_cast<float> (i), plot.getY(), i % 4 == 0 ? 1.0f : 0.6f, plot.getHeight());
    }
    const float rowH = plot.getHeight() / static_cast<float> (top + 2);
    const bool on = value (0) > 0.5f;
    for (int i = 0; i < 16; ++i)
    {
        const int n = seq[static_cast<size_t> (i) % seq.size()];
        const float x = plot.getX() + colW * (static_cast<float> (i) + (i % 2 == 1 ? swing : 0.0f));
        const float y = plot.getBottom() - rowH * static_cast<float> (n + 1) - rowH * 0.5f;
        const bool current = on && i == shownStep;
        const auto c = current ? colours::accent : (on ? colours::ink.withAlpha (0.75f) : colours::caption);
        g.setColour (c.withAlpha (current ? 1.0f : 0.85f));
        g.fillRoundedRectangle (x + 1.0f, y - 3.0f, juce::jmax (3.0f, colW * gate - 2.0f), 6.0f, 3.0f);
        if (current)
        {
            g.setColour (colours::accent.withAlpha (0.15f));
            g.fillRoundedRectangle (x - 2.0f, y - 7.0f, colW * gate + 4.0f, 14.0f, 6.0f);
        }
    }
    drawTracked (g, on ? juce::String ("RUNNING") : juce::String ("ARP OFF  -  PATTERN PREVIEW"), { plot.getX(), r.getY() + 3.0f, 300.0f, 12.0f },
                 Fonts::jost (8.0f, true, 0.18f), on ? colours::accent : colours::caption, juce::Justification::centredLeft);
}

//==============================================================================
Keys::Keys (std::function<void (int, float, bool)> s, std::function<bool (int)> held) : send (std::move (s)), isHeld (std::move (held)) {}

bool Keys::isBlack (int note) noexcept
{
    const int pc = note % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

void Keys::setLowestOctave (int octave)
{
    lowestOctave = juce::jlimit (-1, 5, octave);
    repaint();
}

juce::Rectangle<float> Keys::keyBounds (int note) const
{
    const int whiteCount = numOctaves * 7 + 1;
    const float ww = static_cast<float> (getWidth()) / static_cast<float> (whiteCount);
    const float h = static_cast<float> (getHeight());
    static constexpr int whiteIndex[12] { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    const int rel = note - firstNote();
    const int oct = rel / 12, pc = rel % 12;
    const float x = (static_cast<float> (oct * 7 + whiteIndex[pc])) * ww;
    if (! isBlack (note))
        return { x, 0.0f, ww, h };
    const float bw = ww * 0.62f;
    return { x + ww - bw * 0.5f, 0.0f, bw, h * 0.62f };
}

int Keys::noteAt (juce::Point<float> p, float& velocity) const
{
    const int first = firstNote(), last = first + numOctaves * 12;
    for (int n = first; n <= last; ++n)
        if (isBlack (n) && keyBounds (n).contains (p))
        {
            velocity = juce::jlimit (0.1f, 1.0f, p.y / keyBounds (n).getHeight());
            return n;
        }
    for (int n = first; n <= last; ++n)
        if (! isBlack (n) && keyBounds (n).contains (p))
        {
            velocity = juce::jlimit (0.1f, 1.0f, p.y / keyBounds (n).getHeight());
            return n;
        }
    return -1;
}

void Keys::mouseDown (const juce::MouseEvent& e)
{
    float vel = 0.8f;
    playing = noteAt (e.position, vel);
    if (playing >= 0 && send)
        send (playing, vel, true);
}

void Keys::mouseDrag (const juce::MouseEvent& e)
{
    float vel = 0.8f;
    const int n = noteAt (e.position, vel);
    if (n == playing)
        return;
    if (playing >= 0 && send)
        send (playing, 0.0f, false);
    playing = n;
    if (playing >= 0 && send)
        send (playing, vel, true);
}

void Keys::mouseUp (const juce::MouseEvent&)
{
    if (playing >= 0 && send)
        send (playing, 0.0f, false);
    playing = -1;
}

void Keys::poll()
{
    bool changed = false;
    for (int n = 0; n < 128; ++n)
    {
        const bool h = isHeld && isHeld (n);
        if (h != shownHeld[static_cast<size_t> (n)])
        {
            shownHeld[static_cast<size_t> (n)] = h;
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void Keys::paint (juce::Graphics& g)
{
    const int first = firstNote(), last = first + numOctaves * 12;
    for (int n = first; n <= last; ++n)
    {
        if (isBlack (n))
            continue;
        const auto k = keyBounds (n).reduced (0.5f, 0.0f);
        const bool down = shownHeld[static_cast<size_t> (n)] || n == playing;
        g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xfff6e3d8) : juce::Colours::white, 0.0f, k.getY(),
                                                 down ? juce::Colour (0xffefcdbb) : juce::Colour (0xfff3f0ea), 0.0f, k.getBottom(), false));
        g.fillRoundedRectangle (k, 4.0f);
        g.setColour (colours::panelBorder);
        g.drawRoundedRectangle (k, 4.0f, 1.0f);
        if (down)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (k.getX() + 6.0f, k.getBottom() - 10.0f, k.getWidth() - 12.0f, 3.0f, 1.5f);
        }
        if (n % 12 == 0)
            drawTracked (g, "C" + juce::String (n / 12 - 2), { k.getX(), k.getBottom() - 28.0f, k.getWidth(), 12.0f }, Fonts::jost (8.0f, true, 0.1f),
                         colours::caption, juce::Justification::centred);
    }
    for (int n = first; n <= last; ++n)
    {
        if (! isBlack (n))
            continue;
        const auto k = keyBounds (n);
        const bool down = shownHeld[static_cast<size_t> (n)] || n == playing;
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (k.translated (1.0f, 2.0f), 3.0f);
        g.setGradientFill (juce::ColourGradient (down ? colours::accent : juce::Colour (0xff3a3a3e), 0.0f, k.getY(),
                                                 down ? colours::accent.darker (0.3f) : juce::Colour (0xff1e1e21), 0.0f, k.getBottom(), false));
        g.fillRoundedRectangle (k, 3.0f);
    }
}

} // namespace augur5::ui
