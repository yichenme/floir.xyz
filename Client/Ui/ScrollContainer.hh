#pragma once

#include <Client/Ui/Container.hh>
#include <Client/Ui/Element.hh>

namespace Ui {

    class ScrollBar : public Element {
    public:
        ScrollBar();

        virtual void on_event(uint8_t) override;
    };

    class ScrollContainer : public HContainer {
    public:
        float lerp_scroll;
        float max_height;   // cap; actual height shrinks to fit shorter content
        ScrollContainer(Element *, float);

        virtual void on_render(Renderer &) override;
        virtual void poll_events(ScreenEvent const &) override;
        virtual void refactor() override;
        virtual float get_target_width() const override;
        virtual float get_target_height() const override;
    };
}