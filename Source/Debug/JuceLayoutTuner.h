#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace juce_layout_tuner
{
class Overlay final : public juce::Component, private juce::KeyListener
{
public:
    Overlay (juce::Component& rootComponent, juce::Rectangle<int> referenceCanvas, int grid = 8)
        : root (rootComponent), reference (referenceCanvas), gridSize (juce::jmax (1, grid))
    {
        setInterceptsMouseClicks (true, false);
        setWantsKeyboardFocus (true);
        setAlwaysOnTop (true);
        root.addKeyListener (this);
    }

    ~Overlay() override
    {
        root.removeKeyListener (this);
    }

    void addTarget (juce::Component& component, juce::String stableId)
    {
        jassert (component.getParentComponent() == &root);
        targets.push_back ({ &component, std::move (stableId) });
    }

    void activate (bool shouldBeActive)
    {
        active = shouldBeActive;
        setVisible (active);
        if (active)
        {
            toFront (false);
            grabKeyboardFocus();
        }
        else
        {
            root.grabKeyboardFocus();
        }
        repaint();
    }

    bool isActive() const noexcept { return active; }

    void paint (juce::Graphics& g) override
    {
        if (! active || getWidth() <= 0 || getHeight() <= 0)
            return;

        drawGrid (g);
        drawTargets (g);
        drawHud (g);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();
        selected = findTargetAt (event.position.toInt());
        resizing = false;

        if (auto* target = selectedTarget())
        {
            dragOrigin = event.position;
            dragStart = toReference (target->component->getBounds());
            resizing = resizeHandleFor (*target).contains (event.position.toInt());
            status = resizing ? "Resize" : "Move";
        }
        else
        {
            status = "No component selected";
        }

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        auto* target = selectedTarget();
        if (target == nullptr)
            return;

        const auto dx = juce::roundToInt ((event.position.x - dragOrigin.x) * (float) reference.getWidth() / (float) getWidth());
        const auto dy = juce::roundToInt ((event.position.y - dragOrigin.y) * (float) reference.getHeight() / (float) getHeight());
        auto next = dragStart;

        if (resizing)
            next.setSize (juce::jmax (4, dragStart.getWidth() + dx), juce::jmax (4, dragStart.getHeight() + dy));
        else
            next.setPosition (dragStart.getX() + dx, dragStart.getY() + dy);

        if (! event.mods.isAltDown())
            next = snap (next, resizing);

        target->component->setBounds (fromReference (next));
        root.repaint();
        status = describe (*target);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (auto* target = selectedTarget())
            status = describe (*target);
        repaint();
    }

private:
    struct Target
    {
        juce::Component* component = nullptr;
        juce::String id;
    };

    bool keyPressed (const juce::KeyPress& key) override
    {
        return handleKey (key);
    }

    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        return handleKey (key);
    }

    bool handleKey (const juce::KeyPress& key)
    {
        if (key.isKeyCode (juce::KeyPress::F2Key))
        {
            activate (! active);
            return true;
        }

        if (! active)
            return false;

        if (key.isKeyCode (juce::KeyPress::escapeKey))
        {
            selected = -1;
            status = "Selection cleared";
            repaint();
            return true;
        }

        const auto character = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
        if (character == 'e')
        {
            exportAll();
            return true;
        }
        if (character == 'c')
        {
            exportSelectedCpp();
            return true;
        }

        const auto code = key.getKeyCode();
        if (code != juce::KeyPress::leftKey && code != juce::KeyPress::rightKey
            && code != juce::KeyPress::upKey && code != juce::KeyPress::downKey)
            return false;

        auto* target = selectedTarget();
        if (target == nullptr)
            return true;

        const auto step = key.getModifiers().isShiftDown() ? gridSize : 1;
        const auto dx = code == juce::KeyPress::leftKey ? -step : code == juce::KeyPress::rightKey ? step : 0;
        const auto dy = code == juce::KeyPress::upKey ? -step : code == juce::KeyPress::downKey ? step : 0;
        auto next = toReference (target->component->getBounds());
        const auto resize = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();

        if (resize)
            next.setSize (juce::jmax (4, next.getWidth() + dx), juce::jmax (4, next.getHeight() + dy));
        else
            next.translate (dx, dy);

        target->component->setBounds (fromReference (next));
        root.repaint();
        status = describe (*target);
        repaint();
        return true;
    }

    int findTargetAt (juce::Point<int> point) const
    {
        int result = -1;
        auto smallestArea = std::numeric_limits<int>::max();

        for (int i = 0; i < (int) targets.size(); ++i)
        {
            const auto& target = targets[(size_t) i];
            if (target.component == nullptr || ! target.component->isVisible())
                continue;

            const auto bounds = target.component->getBounds();
            const auto area = bounds.getWidth() * bounds.getHeight();
            if (bounds.contains (point) && area < smallestArea)
            {
                result = i;
                smallestArea = area;
            }
        }

        return result;
    }

    Target* selectedTarget()
    {
        return juce::isPositiveAndBelow (selected, (int) targets.size()) ? &targets[(size_t) selected] : nullptr;
    }

    const Target* selectedTarget() const
    {
        return juce::isPositiveAndBelow (selected, (int) targets.size()) ? &targets[(size_t) selected] : nullptr;
    }

    juce::Rectangle<int> toReference (juce::Rectangle<int> bounds) const
    {
        if (getWidth() <= 0 || getHeight() <= 0)
            return bounds;
        return { juce::roundToInt ((float) bounds.getX() * reference.getWidth() / getWidth()),
                 juce::roundToInt ((float) bounds.getY() * reference.getHeight() / getHeight()),
                 juce::roundToInt ((float) bounds.getWidth() * reference.getWidth() / getWidth()),
                 juce::roundToInt ((float) bounds.getHeight() * reference.getHeight() / getHeight()) };
    }

    juce::Rectangle<int> fromReference (juce::Rectangle<int> bounds) const
    {
        return { juce::roundToInt ((float) bounds.getX() * getWidth() / reference.getWidth()),
                 juce::roundToInt ((float) bounds.getY() * getHeight() / reference.getHeight()),
                 juce::roundToInt ((float) bounds.getWidth() * getWidth() / reference.getWidth()),
                 juce::roundToInt ((float) bounds.getHeight() * getHeight() / reference.getHeight()) };
    }

    juce::Rectangle<int> snap (juce::Rectangle<int> bounds, bool sizeOnly) const
    {
        const auto snapValue = [this] (int value)
        {
            return juce::roundToInt ((float) value / gridSize) * gridSize;
        };

        if (sizeOnly)
            bounds.setSize (juce::jmax (4, snapValue (bounds.getWidth())),
                            juce::jmax (4, snapValue (bounds.getHeight())));
        else
            bounds.setPosition (snapValue (bounds.getX()), snapValue (bounds.getY()));
        return bounds;
    }

    juce::Rectangle<int> resizeHandleFor (const Target& target) const
    {
        const auto bounds = target.component->getBounds();
        return { bounds.getRight() - 12, bounds.getBottom() - 12, 12, 12 };
    }

    juce::String describe (const Target& target) const
    {
        const auto bounds = toReference (target.component->getBounds());
        return target.id + "  x:" + juce::String (bounds.getX()) + " y:" + juce::String (bounds.getY())
               + " w:" + juce::String (bounds.getWidth()) + " h:" + juce::String (bounds.getHeight());
    }

    void drawGrid (juce::Graphics& g) const
    {
        for (int x = 0; x <= reference.getWidth(); x += gridSize)
        {
            const auto px = (float) x * getWidth() / reference.getWidth();
            const auto major = x % (gridSize * 5) == 0;
            g.setColour (juce::Colour (0xff36d7ff).withAlpha (major ? 0.24f : 0.10f));
            g.drawVerticalLine (juce::roundToInt (px), 0.0f, (float) getHeight());
        }
        for (int y = 0; y <= reference.getHeight(); y += gridSize)
        {
            const auto py = (float) y * getHeight() / reference.getHeight();
            const auto major = y % (gridSize * 5) == 0;
            g.setColour (juce::Colour (0xff36d7ff).withAlpha (major ? 0.24f : 0.10f));
            g.drawHorizontalLine (juce::roundToInt (py), 0.0f, (float) getWidth());
        }
    }

    void drawTargets (juce::Graphics& g) const
    {
        for (int i = 0; i < (int) targets.size(); ++i)
        {
            const auto& target = targets[(size_t) i];
            if (target.component == nullptr || ! target.component->isVisible())
                continue;

            const auto bounds = target.component->getBounds().toFloat();
            const auto isSelected = i == selected;
            g.setColour (isSelected ? juce::Colour (0xffffb547) : juce::Colour (0xff36d7ff).withAlpha (0.42f));
            g.drawRect (bounds, isSelected ? 2.0f : 1.0f);

            if (isSelected)
            {
                const auto handle = resizeHandleFor (target).toFloat();
                g.setColour (juce::Colour (0xffffb547));
                g.fillRect (handle);
                g.setColour (juce::Colour (0xff07090b));
                g.drawLine (handle.getX() + 3.0f, handle.getBottom() - 3.0f,
                            handle.getRight() - 3.0f, handle.getY() + 3.0f, 1.0f);
            }
        }
    }

    void drawHud (juce::Graphics& g) const
    {
        auto hud = juce::Rectangle<float> (12.0f, 12.0f, 420.0f, 54.0f);
        g.setColour (juce::Colour (0xee07090b));
        g.fillRoundedRectangle (hud, 5.0f);
        g.setColour (juce::Colour (0xff36d7ff));
        g.drawRoundedRectangle (hud, 5.0f, 1.0f);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.setColour (juce::Colours::white);
        g.drawText ("F2 toggle | drag move | corner resize | arrows nudge | C selected | E export",
                    hud.removeFromTop (25.0f).reduced (8.0f, 2.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffffb547));
        g.drawText (status.isNotEmpty() ? status : "Click a registered component",
                    hud.reduced (8.0f, 2.0f), juce::Justification::centredLeft);
    }

    void exportSelectedCpp()
    {
        auto* target = selectedTarget();
        if (target == nullptr)
        {
            status = "Select a component before pressing C";
            repaint();
            return;
        }

        const auto b = toReference (target->component->getBounds());
        const auto text = target->id + ".setBounds (bounds (" + juce::String (b.getX()) + ".0f, "
                          + juce::String (b.getY()) + ".0f, " + juce::String (b.getWidth()) + ".0f, "
                          + juce::String (b.getHeight()) + ".0f));";
        juce::SystemClipboard::copyTextToClipboard (text);
        status = "Copied: " + target->id;
        repaint();
    }

    void exportAll()
    {
        auto* rootObject = new juce::DynamicObject();
        rootObject->setProperty ("format", "juce-layout-tuner-v1");
        rootObject->setProperty ("referenceWidth", reference.getWidth());
        rootObject->setProperty ("referenceHeight", reference.getHeight());

        juce::Array<juce::var> components;
        for (const auto& target : targets)
        {
            if (target.component == nullptr)
                continue;
            const auto b = toReference (target.component->getBounds());
            auto* item = new juce::DynamicObject();
            item->setProperty ("id", target.id);
            item->setProperty ("x", b.getX());
            item->setProperty ("y", b.getY());
            item->setProperty ("width", b.getWidth());
            item->setProperty ("height", b.getHeight());
            components.add (juce::var (item));
        }
        rootObject->setProperty ("components", components);

        juce::SystemClipboard::copyTextToClipboard (juce::JSON::toString (juce::var (rootObject), true));
        status = "Copied " + juce::String (components.size()) + " component bounds";
        repaint();
    }

    juce::Component& root;
    juce::Rectangle<int> reference;
    int gridSize = 8;
    std::vector<Target> targets;
    int selected = -1;
    bool active = true;
    bool resizing = false;
    juce::Point<float> dragOrigin;
    juce::Rectangle<int> dragStart;
    juce::String status;
};
} // namespace juce_layout_tuner
