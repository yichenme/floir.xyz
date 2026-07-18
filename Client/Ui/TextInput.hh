#pragma once

#include <Client/Ui/Element.hh>

#include <string>

namespace Ui {
    class TextInput : public Element {
        std::string const name;
        std::string &ref;
        uint32_t max;
        float font_scale;
    public:
        TextInput(std::string &, float, float, uint32_t, Style = {}, bool password = false, char const *placeholder = nullptr, float font_scale = 1.0f);

        virtual void on_render(Renderer &) override;
        virtual void on_render_skip(Renderer &) override;

        void clear();   // empty the backing text + DOM element
    };
}