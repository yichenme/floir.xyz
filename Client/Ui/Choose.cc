#include <Client/Ui/Choose.hh>

#include <Client/Ui/Extern.hh>

#include <Helpers/Macros.hh>

using namespace Ui;

uint8_t no_show() { return 0; }
uint8_t do_show() { return 1; }

Choose::Choose(Element *l, Element *r, std::function<uint8_t(void)> const &c, Style s) : 
    Container({l, r},0,0,s), chooser(std::move(c)), choose_showing(c())
{
    l->showed = 1;
    r->showed = 1;
    l->refactor();
    r->refactor();
    r->animation.set(0);
    refactor();
}

void Choose::on_render(Renderer &ctx) {
    Element::on_render(ctx);
    for (Element *child : children) {
        child->render(ctx);
    }
}

void Choose::refactor() {
    uint8_t to_show = chooser();
    children[to_show]->style.should_render = do_show;
    children[1 - to_show]->style.should_render = no_show;
    for (Element *child : children) {
        if (child->visible || child->style.should_render() || (float)child->animation > 0.001) {
            child->refactor();
        }
    }
    float anim0 = (float)children[0]->animation;
    float anim1 = (float)children[1]->animation;
    float target_w = children[0]->width * anim0 + children[1]->width * anim1;
    float target_h = children[0]->height * anim0 + children[1]->height * anim1;
    if (width == 0 || height == 0 || style.no_animation) {
        width = target_w;
        height = target_h;
    } else {
        width = lerp(width, target_w, Ui::lerp_amount);
        height = lerp(height, target_h, Ui::lerp_amount);
    }
    if (!children[choose_showing]->visible || !showed)
        choose_showing = to_show;
}

void Choose::poll_events(ScreenEvent const &event) {
    if (style.no_polling) return;
    Element::poll_events(event);
    if (Ui::focused != this)
        return;
    if (children[0]->visible) children[0]->poll_events(event);
    if (children[1]->visible) children[1]->poll_events(event);
}

float Choose::get_target_width() const {
    uint8_t to_show = chooser();
    return children[to_show]->width;
}

float Choose::get_target_height() const {
    uint8_t to_show = chooser();
    return children[to_show]->height;
}