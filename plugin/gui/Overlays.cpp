#include "Overlays.h"

#include "../Parameters.h"

namespace augur5::ui
{

namespace
{
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
} // namespace

//==============================================================================
void Overlay::open()
{
    setVisible (true);
    toFront (true);
    opened();
    grabKeyboardFocus();
}

void Overlay::close()
{
    setVisible (false);
    if (onClosed)
        onClosed();
}

void Overlay::mouseDown (const juce::MouseEvent& e)
{
    if (! panel.contains (e.getPosition()))
        close(); // click on the backdrop
}

bool Overlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

void Overlay::paintCard (juce::Graphics& g, const juce::String& title) const
{
    // A white veil over the instrument, then the card.
    g.fillAll (juce::Colour (0xffe9e6e0).withAlpha (0.72f));
    const auto r = panel.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.06f));
    g.fillRoundedRectangle (r.translated (0.0f, 8.0f).expanded (6.0f), 16.0f);
    g.setColour (juce::Colours::black.withAlpha (0.07f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f).expanded (1.0f), 12.0f);
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (colours::panelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    drawTracked (g, title, { r.getX() + 28.0f, r.getY() + 18.0f, 360.0f, 30.0f }, Fonts::light (22.0f, 0.3f), colours::title,
                 juce::Justification::centredLeft);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (r.getX() + 28.0f, r.getY() + 52.0f, 22.0f, 2.0f, 1.0f);
}

//==============================================================================
struct PresetBrowser::ListModel final : juce::ListBoxModel
{
    std::function<int()> count;
    std::function<void (juce::Graphics&, int, int, int, bool)> paint;
    std::function<void (int)> selected, chosen;
    bool silent = false;

    int getNumRows() override { return count ? count() : 0; }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (paint)
            paint (g, row, w, h, sel);
    }
    void selectedRowsChanged (int row) override
    {
        if (! silent && row >= 0 && selected)
            selected (row);
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (chosen)
            chosen (row);
    }
    void returnKeyPressed (int row) override
    {
        if (chosen)
            chosen (row);
    }
};

namespace
{
void styleList (juce::ListBox& list, int rowHeight)
{
    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, colours::panelBottom);
    list.setColour (juce::ListBox::outlineColourId, colours::subPanelBorder);
    list.setOutlineThickness (1);
    auto& bar = list.getVerticalScrollBar();
    bar.setColour (juce::ScrollBar::thumbColourId, colours::accent.withAlpha (0.45f));
    bar.setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
}

void paintRow (juce::Graphics& g, int w, int h, bool selected, bool current, const juce::String& text, const juce::String& tag)
{
    if (selected)
    {
        g.setColour (colours::accent.withAlpha (0.08f));
        g.fillRect (0, 0, w, h);
        g.setColour (colours::accent);
        g.fillRect (0, 4, 2, h - 8);
    }
    else
    {
        g.setColour (colours::hairline);
        g.fillRect (14, h - 1, w - 28, 1);
    }
    const auto colour = current ? colours::accent.darker (0.15f) : (selected ? colours::title : colours::label);
    drawTracked (g, text, { 16.0f, 0.0f, static_cast<float> (w) - (tag.isEmpty() ? 24.0f : 210.0f), static_cast<float> (h) },
                 Fonts::jost (13.0f, current || selected, 0.02f), colour, juce::Justification::centredLeft);
    if (tag.isNotEmpty())
        drawTracked (g, tag, { static_cast<float> (w) - 200.0f, 0.0f, 186.0f, static_cast<float> (h) }, Fonts::jost (9.5f, false, 0.14f),
                     colours::caption, juce::Justification::centredRight);
}
} // namespace

PresetBrowser::PresetBrowser (PresetManager& pm, Actions a) : presets (pm), actions (std::move (a))
{
    setWantsKeyboardFocus (true);
    panel = { 168, 92, 1200, 836 };

    collectionModel = std::make_unique<ListModel>();
    collectionModel->count = [this] { return collections.size(); };
    collectionModel->paint = [this] (juce::Graphics& g, int row, int w, int h, bool sel) { paintRow (g, w, h, sel, false, collections[row], {}); };
    collectionModel->selected = [this] (int row) {
        collection = row;
        category = 0;
        refreshCategories();
        refreshPresets();
    };

    categoryModel = std::make_unique<ListModel>();
    categoryModel->count = [this] { return categories.size(); };
    categoryModel->paint = [this] (juce::Graphics& g, int row, int w, int h, bool sel) { paintRow (g, w, h, sel, false, categories[row], {}); };
    categoryModel->selected = [this] (int row) {
        category = row;
        refreshPresets();
    };

    presetModel = std::make_unique<ListModel>();
    presetModel->count = [this] { return static_cast<int> (visible.size()); };
    presetModel->paint = [this] (juce::Graphics& g, int row, int w, int h, bool sel) {
        if (row < 0 || row >= static_cast<int> (visible.size()))
            return;
        const auto& e = entries[static_cast<size_t> (visible[static_cast<size_t> (row)])];
        const bool searching = search.getText().trim().isNotEmpty();
        paintRow (g, w, h, sel, e.name == presets.getCurrentName(), e.name, searching ? e.collection + u8 ("  \xc2\xb7  ") + e.category : e.category);
    };
    presetModel->selected = [this] (int row) { load (row, false); };
    presetModel->chosen = [this] (int row) { load (row, true); };

    collectionList.setModel (collectionModel.get());
    categoryList.setModel (categoryModel.get());
    presetList.setModel (presetModel.get());
    styleList (collectionList, 32);
    styleList (categoryList, 32);
    styleList (presetList, 32);
    for (auto* l : { &collectionList, &categoryList, &presetList })
        addAndMakeVisible (*l);

    search.setFont (Fonts::jost (13.0f, false, 0.02f));
    search.setTextToShowWhenEmpty ("Search presets...", colours::caption);
    search.setColour (juce::TextEditor::backgroundColourId, colours::panelBottom);
    search.setColour (juce::TextEditor::outlineColourId, colours::subPanelBorder);
    search.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent.withAlpha (0.7f));
    search.setColour (juce::TextEditor::textColourId, colours::text);
    search.setIndents (12, 9);
    search.onTextChange = [this] { refreshPresets(); };
    search.onReturnKey = [this] {
        if (! visible.empty())
            load (juce::jmax (0, presetList.getSelectedRow()), true);
    };
    search.onEscapeKey = [this] { close(); };
    addAndMakeVisible (search);

    const auto add = [this] (const char* text, juce::Colour c, std::function<void()> fn) {
        buttons.push_back (std::make_unique<ActionButton> (text, c, std::move (fn)));
        addAndMakeVisible (*buttons.back());
    };
    add ("SAVE PRESET", colours::accent, [this] { close(); if (actions.save) actions.save(); });
    add ("INSTALL PACK (.ZIP)", colours::slate, [this] { close(); if (actions.install) actions.install(); });
    add ("ADD PRESETS FOLDER", colours::slate, [this] { close(); if (actions.addFolder) actions.addFolder(); });
    add ("OPEN PRESETS FOLDER", colours::slate, [] {
        auto folder = PresetManager::getUserFolder();
        folder.createDirectory();
        folder.startAsProcess();
    });
    add ("CLOSE", colours::accent, [this] { close(); });
}

PresetBrowser::~PresetBrowser()
{
    collectionList.setModel (nullptr);
    categoryList.setModel (nullptr);
    presetList.setModel (nullptr);
}

void PresetBrowser::resized()
{
    const auto p = panel;
    search.setBounds (p.getRight() - 28 - 520, p.getY() + 20, 520, 36);
    collectionList.setBounds (p.getX() + 28, p.getY() + 100, 300, 224);
    categoryList.setBounds (p.getX() + 28, p.getY() + 364, 300, p.getHeight() - 364 - 84);
    presetList.setBounds (p.getX() + 352, p.getY() + 100, p.getWidth() - 352 - 28, p.getHeight() - 100 - 84);
    const int by = p.getBottom() - 60;
    const int widths[] { 150, 190, 190, 200 };
    int x = p.getX() + 28;
    for (size_t i = 0; i < 4; ++i)
    {
        buttons[i]->setBounds (x, by, widths[i], 34);
        x += widths[i] + 12;
    }
    buttons[4]->setBounds (p.getRight() - 28 - 120, by, 120, 34);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    paintCard (g, "PRESETS");
    const auto p = panel.toFloat();
    const auto caption = [&] (const juce::String& t, float x, float y) {
        drawTracked (g, t, { x, y, 300.0f, 16.0f }, Fonts::jost (10.0f, true, 0.25f), colours::captionLight, juce::Justification::centredLeft);
    };
    caption ("COLLECTION", p.getX() + 30.0f, p.getY() + 78.0f);
    caption ("CATEGORY", p.getX() + 30.0f, p.getY() + 342.0f);
    caption (juce::String (static_cast<int> (visible.size())) + " PRESETS", p.getX() + 354.0f, p.getY() + 78.0f);
    drawTracked (g, u8 ("Click to audition  \xc2\xb7  double-click or Enter to load  \xc2\xb7  \xe2\x86\x91 \xe2\x86\x93 to step  \xc2\xb7  Esc to close"),
                 { p.getRight() - 560.0f, p.getY() + 74.0f, 532.0f, 20.0f }, Fonts::jost (9.5f, false, 0.05f), colours::caption,
                 juce::Justification::centredRight);
}

void PresetBrowser::openOnCategory (const juce::String& c)
{
    pendingCategory = c;
    open();
}

void PresetBrowser::opened()
{
    rescan();
    const auto current = presets.getCurrentName();
    collection = 0;
    category = 0;
    if (pendingCategory.isNotEmpty())
    {
        refreshCategories();
        category = juce::jmax (0, categories.indexOf (pendingCategory));
        pendingCategory.clear();
    }
    else
    {
        for (const auto& e : entries)
            if (e.name == current)
            {
                collection = juce::jmax (0, collections.indexOf (e.collection));
                refreshCategories();
                category = juce::jmax (0, categories.indexOf (e.category));
                break;
            }
    }
    collectionModel->silent = categoryModel->silent = true;
    collectionList.updateContent();
    collectionList.selectRow (collection);
    refreshCategories();
    categoryList.selectRow (category);
    collectionModel->silent = categoryModel->silent = false;
    search.clear();
    refreshPresets();
    presetList.grabKeyboardFocus();
}

void PresetBrowser::rescan()
{
    entries.clear();
    collections.clear();
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
        entries.push_back ({ presets.getFactoryName (i), "FACTORY", presets.getFactoryCategory (i), i, {} });
    collections.add ("FACTORY");

    const auto base = PresetManager::getUserFolder();
    juce::StringArray packs;
    bool hasLoose = false;
    for (const auto& f : presets.getUserPresets())
    {
        auto parts = juce::StringArray::fromTokens (f.getParentDirectory().getRelativePathFrom (base).replaceCharacter ('\\', '/'), "/", "");
        parts.removeEmptyStrings();
        parts.removeString (".");
        Entry e;
        e.name = f.getFileNameWithoutExtension();
        e.file = f;
        if (parts.isEmpty())
        {
            e.collection = "USER";
            e.category = "USER";
            hasLoose = true;
        }
        else
        {
            e.collection = parts[0];
            e.category = parts.size() > 1 ? parts[1] : juce::String ("ALL");
            packs.addIfNotAlreadyThere (parts[0]);
        }
        entries.push_back (e);
    }
    packs.sort (true);
    collections.addArray (packs);
    if (hasLoose)
        collections.add ("USER");
}

void PresetBrowser::refreshCategories()
{
    categories.clear();
    categories.add ("ALL");
    const auto& col = collections[collection];
    juce::StringArray found;
    for (const auto& e : entries)
        if (e.collection == col)
            found.addIfNotAlreadyThere (e.category);
    if (col != "FACTORY")
        found.sort (true);
    categories.addArray (found);
    categoryModel->silent = true;
    categoryList.updateContent();
    categoryList.selectRow (juce::jlimit (0, categories.size() - 1, category));
    categoryModel->silent = false;
}

void PresetBrowser::refreshPresets()
{
    visible.clear();
    const auto query = search.getText().trim();
    const auto& col = collections[collection];
    const auto& cat = category > 0 ? categories[category] : juce::String();
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (query.isNotEmpty())
        {
            if (e.name.containsIgnoreCase (query) || e.category.containsIgnoreCase (query) || e.collection.containsIgnoreCase (query))
                visible.push_back (static_cast<int> (i)); // search spans every collection
        }
        else if (e.collection == col && (cat.isEmpty() || e.category == cat))
        {
            visible.push_back (static_cast<int> (i));
        }
    }
    presetModel->silent = true;
    presetList.updateContent();
    presetList.deselectAllRows();
    for (size_t r = 0; r < visible.size(); ++r)
        if (entries[static_cast<size_t> (visible[r])].name == presets.getCurrentName())
        {
            presetList.selectRow (static_cast<int> (r));
            break;
        }
    presetModel->silent = false;
    repaint();
}

void PresetBrowser::load (int row, bool andClose)
{
    if (row < 0 || row >= static_cast<int> (visible.size()))
        return;
    const auto& e = entries[static_cast<size_t> (visible[static_cast<size_t> (row)])];
    if (e.factory >= 0)
        presets.loadFactory (e.factory);
    else
        presets.loadUser (e.file);
    presetList.repaint();
    if (andClose)
        close();
}

//==============================================================================
SettingsPanel::SettingsPanel (APVTS& s, Actions a) : state (s), actions (std::move (a))
{
    setWantsKeyboardFocus (true);
    panel = { 944, 82, 540, 676 };
    const int x = panel.getX() + 28, w = panel.getWidth() - 56;
    const auto add = [this] (std::unique_ptr<juce::Component> c, juce::Rectangle<int> r) {
        c->setBounds (r);
        addAndMakeVisible (*c);
        owned.push_back (std::move (c));
        return owned.back().get();
    };
    const auto choiceRow = [&] (const char* id, std::vector<const char*> labels, int y, juce::Colour c) {
        std::vector<std::unique_ptr<juce::Button>> b;
        for (auto* l : labels)
            b.push_back (std::make_unique<SegmentButton> (u8 (l), c));
        auto group = std::make_unique<ChoiceGroup> (state, id, std::move (b));
        const int n = static_cast<int> (labels.size());
        const int bw = (w - 6 * (n - 1)) / n;
        for (int i = 0; i < n; ++i)
            group->getButton (i).setBounds (i * (bw + 6), 0, bw, 30);
        add (std::move (group), { x, y, w, 30 });
    };

    // WINDOW SIZE
    const float sizes[] = { 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.25f };
    const int bw = (w - 5 * 6) / 6;
    for (int i = 0; i < 6; ++i)
    {
        const float sc = sizes[i];
        auto* b = static_cast<ActionButton*> (add (std::make_unique<ActionButton> (juce::String (juce::roundToInt (sc * 100.0f)) + "%", colours::accent,
                                                                                    [this, sc] { actions.setScale (sc); opened(); }),
                                                   { x + i * (bw + 6), panel.getY() + 98, bw, 30 }));
        sizeButtons.emplace_back (b, sc);
    }
    add (std::make_unique<ActionButton> ("FIT TO SCREEN", colours::accent, [this] { actions.fitScale(); opened(); }), { x, panel.getY() + 136, w, 30 });

    // SOUND
    choiceRow (params::quality, { "ECO", "GREAT", "DIVINE" }, panel.getY() + 214, colours::slate);
    add (std::make_unique<ParamToggle> (state, params::offline_quality, "RENDER OFFLINE IN DIVINE", colours::slate), { x, panel.getY() + 252, w, 30 });

    // CIRCUIT
    choiceRow (params::osc_model, { "REV 3  \xc2\xb7  CEM3340", "REV 1  \xc2\xb7  SSM2030" }, panel.getY() + 330, colours::accent);
    add (std::make_unique<ParamToggle> (state, params::vintage_cv, "VINTAGE 7-BIT KNOBS (REV 3)", colours::accent), { x, panel.getY() + 368, w, 30 });

    // DISPLAY
    idsButton = static_cast<ActionButton*> (add (std::make_unique<ActionButton> ("SHOW PARAMETER IDS", colours::plum, [this] {
                                                     showIds = ! showIds;
                                                     if (actions.refreshIds)
                                                         actions.refreshIds();
                                                     opened();
                                                 }),
                                                 { x, panel.getY() + 446, w, 30 }));

    // PRESETS
    const int hw = (w - 6) / 2;
    add (std::make_unique<ActionButton> ("SAVE PRESET...", colours::accent, [this] { close(); if (actions.save) actions.save(); }),
         { x, panel.getY() + 524, hw, 30 });
    add (std::make_unique<ActionButton> ("INSTALL PACK (.ZIP)...", colours::slate, [this] { close(); if (actions.install) actions.install(); }),
         { x + hw + 6, panel.getY() + 524, hw, 30 });
    add (std::make_unique<ActionButton> ("ADD PRESETS FOLDER...", colours::slate, [this] { close(); if (actions.addFolder) actions.addFolder(); }),
         { x, panel.getY() + 562, hw, 30 });
    add (std::make_unique<ActionButton> ("OPEN PRESETS FOLDER", colours::slate, [this] { if (actions.openFolder) actions.openFolder(); }),
         { x + hw + 6, panel.getY() + 562, hw, 30 });
}

void SettingsPanel::opened()
{
    const float current = actions.currentScale ? actions.currentScale() : 1.0f;
    for (auto& [b, sc] : sizeButtons)
        b->setToggleState (std::abs (current - sc) < 0.01f, juce::dontSendNotification);
    idsButton->setToggleState (showIds, juce::dontSendNotification);
    repaint();
}

void SettingsPanel::paint (juce::Graphics& g)
{
    paintCard (g, "SETTINGS");
    const auto p = panel.toFloat();
    const auto caption = [&] (const juce::String& t, float y) {
        drawTracked (g, t, { p.getX() + 30.0f, p.getY() + y, 400.0f, 16.0f }, Fonts::jost (10.0f, true, 0.25f), colours::captionLight,
                     juce::Justification::centredLeft);
    };
    caption ("WINDOW SIZE", 76.0f);
    caption ("SOUND QUALITY  (OVERSAMPLING)", 192.0f);
    caption ("OSCILLATOR CIRCUIT", 308.0f);
    caption ("DISPLAY", 424.0f);
    caption ("PRESETS", 502.0f);
    drawTracked (g, u8 ("AUGUR-5 \xe2\x80\x9c" "3340\xe2\x80\x9d  v") + JucePlugin_VersionString + u8 ("  \xc2\xb7  TONAL LAB"),
                 { p.getX() + 30.0f, p.getBottom() - 42.0f, p.getWidth() - 60.0f, 18.0f }, Fonts::jost (10.0f, false, 0.15f), colours::caption,
                 juce::Justification::centredLeft);
}

} // namespace augur5::ui
