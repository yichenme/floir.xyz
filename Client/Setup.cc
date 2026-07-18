#include <Client/Setup.hh>

#include <Client/DOM.hh>
#include <Client/Game.hh>
#include <Client/Input.hh>
#include <Client/Render/MapRenderer.hh>
#include <Client/Storage.hh>

#include <unordered_map>

#include <emscripten.h>

// Read by the JS render loop / input handlers to pick render scale + fps cap.
extern "C" EMSCRIPTEN_KEEPALIVE int get_high_quality() { return Input::high_quality; }

static char _get_key_from_code(std::string const &code) {
    static std::unordered_map<std::string, char> const KEYCODE_MAP = 
        {{"AltLeft", 18},{"AltRight", 18},{"ArrowDown", 40},
        {"ArrowLeft", 37},{"ArrowRight", 39},{"ArrowUp", 38},
        {"Backquote", '`'},{"Backslash", '\\'},{"Backspace", 8},
        {"BracketLeft", '['},{"BracketRight", ']'},{"CapsLock", 20},
        {"Comma", ','},{"ControlLeft", 17},{"ControlRight", 17},
        {"Delete", 46},{"Digit0", '0'},{"Digit1", '1'},{"Digit2", '2'},
        {"Digit3", '3'},{"Digit4", '4'},{"Digit5", '5'},{"Digit6", '6'},
        {"Digit7", '7'},{"Digit8", '8'},{"Digit9", '9'},{"Enter", '\r'},
        {"Equal", '='},{"Escape", 27},{"F1", 112},{"F10", 121},
        {"F11", 122},{"F12", 123},{"F2", 113},{"F3", 114},
        {"F4", 115},{"F5", 116},{"F6", 117},{"F7", 118},{"F8", 119},
        {"F9", 120},{"Insert", 45},{"KeyA", 'A'},{"KeyB", 'B'},
        {"KeyC", 'C'},{"KeyD", 'D'},{"KeyE", 'E'},{"KeyF", 'F'},
        {"KeyG", 'G'},{"KeyH", 'H'},{"KeyI", 'I'},{"KeyJ", 'J'},
        {"KeyK", 'K'},{"KeyL", 'L'},{"KeyM", 'M'},{"KeyN", 'N'},
        {"KeyO", 'O'},{"KeyP", 'P'},{"KeyQ", 'Q'},{"KeyR", 'R'},
        {"KeyS", 'S'},{"KeyT", 'T'},{"KeyU", 'U'},{"KeyV", 'V'},
        {"KeyW", 'W'},{"KeyX", 'X'},{"KeyY", 'Y'},{"KeyZ", 'Z'},
        {"MetaLeft", 91},{"Minus", '-'},{"Period", '.'},
        {"Quote", '\''},{"Semicolon", ';'},{"ShiftLeft", '\x10'},
        {"ShiftRight", '\x10'},{"Slash", '/'},{"Space", ' '},{"Tab", 9}};
    auto place = KEYCODE_MAP.find(code);
    if (place == KEYCODE_MAP.end()) return 0;
    return place->second;
}

extern "C" {
    void mouse_event(float x, float y, uint8_t type, uint8_t button) {
        Input::mouse_x = x;
        Input::mouse_y = y;
        if (type == 0) {
            BitMath::set(Input::mouse_buttons_pressed, button);
            BitMath::set(Input::mouse_buttons_state, button);
        }
        else if (type == 2) {
            BitMath::set(Input::mouse_buttons_released, button);
            BitMath::unset(Input::mouse_buttons_state, button);
        }
    }

    void key_event(char *code, uint8_t type) {
        char button = _get_key_from_code(std::string(code));
        if (type == 0) {
            // Rising edge: only when not already held (ignore keyrepeat).
            if (!Input::keys_held.contains(button))
                Input::keys_pressed_this_tick.insert(button);
            Input::keys_held.insert(button);
            Input::keys_held_this_tick.insert(button);
        }
        else if (type == 1) Input::keys_held.erase(button);
        free(code);
    }

    void touch_event(float x, float y, uint8_t type, uint32_t id) {
        if (type == 0) {
            Input::touches.insert({id, { .id = id, .x = x, .y = y, .dx = 0, .dy = 0, .saturated = 0 }});
        } else if (type == 2) {
            Input::touches.erase(id);
        } else {
            auto iter = Input::touches.find(id);
            if (iter == Input::touches.end()) return;
            Input::Touch &touch = iter->second;
            touch.dx = x - touch.x;
            touch.dy = y - touch.y;
            touch.x = x;
            touch.y = y;
        }
    }

    void wheel_event(float wheel) {
        Input::wheel_delta = wheel;
    }

    void clipboard_event(char *clipboard) {
        Input::clipboard = std::string(clipboard);
        free(clipboard);
    }

    void loop(double d, float width, float height) {
        Game::renderer.width = width;
        Game::renderer.height = height;
        Game::tick(d);
    }

}

int setup_inputs() {
    EM_ASM({
        window.addEventListener("keydown", (e) => {
            //e.preventDefault();
            !e.repeat && _key_event(stringToNewUTF8(e.code), 0);
        });
        window.addEventListener("keyup", (e) => {
            //e.preventDefault();
            !e.repeat && _key_event(stringToNewUTF8(e.code), 1);
        });
        window.addEventListener("mousedown", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio * (Module.renderScale||1), e.clientY * devicePixelRatio * (Module.renderScale||1), 0, +!!e.button);
        });
        window.addEventListener("mousemove", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio * (Module.renderScale||1), e.clientY * devicePixelRatio * (Module.renderScale||1), 1, +!!e.button);
        });
        window.addEventListener("mouseup", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio * (Module.renderScale||1), e.clientY * devicePixelRatio * (Module.renderScale||1), 2, +!!e.button);
        });
        window.addEventListener("touchstart", (e) => {
            for (const t of e.changedTouches)
                _touch_event(t.clientX * devicePixelRatio * (Module.renderScale||1), t.clientY * devicePixelRatio * (Module.renderScale||1), 0, t.identifier);
        }, { passive: false });
        window.addEventListener("touchmove", (e) => {
            for (const t of e.changedTouches)
                _touch_event(t.clientX * devicePixelRatio * (Module.renderScale||1), t.clientY * devicePixelRatio * (Module.renderScale||1), 1, t.identifier);
        }, { passive: false });
        window.addEventListener("touchend", (e) => {
            for (const t of e.changedTouches)
                _touch_event(t.clientX * devicePixelRatio * (Module.renderScale||1), t.clientY * devicePixelRatio * (Module.renderScale||1), 2, t.identifier);
        }, { passive: false });
        window.addEventListener("touchcancel", (e) => {
            for (const t of e.changedTouches)
                _touch_event(t.clientX * devicePixelRatio * (Module.renderScale||1), t.clientY * devicePixelRatio * (Module.renderScale||1), 2, t.identifier);
        }, { passive: false });
        window.addEventListener("paste", (e) => {
            try {
                const clip = e.clipboardData.getData("text/plain");
                _clipboard_event(stringToNewUTF8(clip));
            } catch(e) {
            };
        }, { capture: true });
        window.addEventListener("wheel", (e) => {
            _wheel_event(e.deltaY);
        });
    });
    return 0;
}

void main_loop() {
    EM_ASM({
        let lastFrame = -1e9;
        // Switching quality changes renderScale, so the canvas resizes and every
        // UI element must be re-laid-out at the new resolution. Cover that reflow
        // with a brief "Loading" overlay so the user doesn't see the jump.
        let lastHq = _get_high_quality();
        let hideOverlayAt = 0;
        const qOverlay = document.createElement('div');
        qOverlay.style.cssText = 'position:absolute;inset:0;background:#000;color:#fff;font:bold 48pt sans-serif;display:none;align-items:center;justify-content:center;z-index:10;';
        qOverlay.textContent = 'Loading';
        document.body.appendChild(qOverlay);
        function loop(time)
        {
            const hq = _get_high_quality();
            if (hq !== lastHq) {
                lastHq = hq;
                qOverlay.style.display = 'flex';
                hideOverlayAt = time + 450;
            }
            // High quality: full res + 60fps. Low: half res + 30fps.
            const minDelta = hq ? 0 : (1000 / 30) - 1;
            if (time - lastFrame >= minDelta) {
                lastFrame = time;
                Module.renderScale = hq ? 1 : 0.5;
                const s = devicePixelRatio * Module.renderScale;
                const w = Math.max(1, Math.floor(innerWidth * s));
                const h = Math.max(1, Math.floor(innerHeight * s));
                Module.canvas.width = w;
                Module.canvas.height = h;
                _loop(time, w, h);
            }
            // Reveal the re-laid-out frame once a couple of frames have rendered.
            if (hideOverlayAt && time >= hideOverlayAt) {
                qOverlay.style.display = 'none';
                hideOverlayAt = 0;
            }
            requestAnimationFrame(loop);
        };
        requestAnimationFrame(loop);
    });
}

int setup_canvas() {
    EM_ASM({
        Module.canvas = document.getElementById("canvas");
        Module.canvas.width = innerWidth * devicePixelRatio;
        Module.canvas.height = innerHeight * devicePixelRatio;
        Module.canvas.oncontextmenu = function() { return false; };
        window.onbeforeunload = function(e) { return "Are you sure?"; };
        Module.ctxs = [];
        Module.availableCtxs = [];
        Module.TextDecoder = new TextDecoder('utf8');
        // Grass tile for the animated title-screen backdrop.
        Module.grassImage = new Image();
        Module.grassImage.src = 'grass_bg.svg';
    });
    // Map tiles are drawn as vector from map-data.json (see MapRenderer).
    Ui::MapRenderer::load();
    return 0;
}

uint8_t check_mobile() {
    return EM_ASM_INT({
        return /iPhone|iPad|iPod|Android|BlackBerry/i.test(navigator.userAgent);
    });
}