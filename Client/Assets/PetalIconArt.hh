#pragma once

// Loadout/gallery icon art for petals whose in-world model differs from the
// florr-style card icon. Traced from petals-and-mobs/petals/{29,94_8}.svg
// (graphic only -- name text is drawn by draw_loadout_background).
// Do not use these for in-world petal rendering.

#include <Client/Render/Renderer.hh>

namespace MagnetIconArt {
    // Horseshoe from petals/29.svg. Coords relative to that SVG's 110x110
    // centre; content+stroke bbox centre ≈ (-6.645, -0.76), max extent ≈ 39.
    inline void draw(Renderer &ctx, float r) {
        RenderContext _c(&ctx);
        ctx.scale(r / 32.0f);
        ctx.translate(6.645f, 0.76f);
        ctx.round_line_cap();
        ctx.round_line_join();
        ctx.set_line_width(24);
        ctx.set_stroke(0xff363685);
        ctx.begin_path();
        ctx.move_to(19.7f, -9.27f);
        ctx.qcurve_to(10.06f, 24.78f, -11.58f, 10.89f);
        ctx.stroke();
        ctx.set_line_width(14.4f);
        ctx.set_stroke(0xff4343a4);
        ctx.begin_path();
        ctx.move_to(19.93f, -9.63f);
        ctx.qcurve_to(10.06f, 24.78f, -11.58f, 10.89f);
        ctx.stroke();
        ctx.set_line_width(24);
        ctx.set_stroke(0xff853636);
        ctx.begin_path();
        ctx.move_to(-6.27f, -25.94f);
        ctx.qcurve_to(-33.22f, -3.01f, -11.58f, 10.89f);
        ctx.stroke();
        ctx.set_line_width(14.4f);
        ctx.set_stroke(0xffa44343);
        ctx.begin_path();
        ctx.move_to(-6.04f, -26.3f);
        ctx.qcurve_to(-33.22f, -3.01f, -11.58f, 10.89f);
        ctx.stroke();
    }
}

namespace MjolnirIconArt {
    // Unique Mjolnir card icon from petals/94_8.svg (handle + head + star).
    // Shifted up so the long handle clears the name row drawn under the icon.
    inline void draw(Renderer &ctx, float r) {
        RenderContext _c(&ctx);
        ctx.scale(r / 34.0f);
        ctx.translate(0, -8.0f);
        ctx.round_line_join();

        // Handle
        ctx.set_fill(0xff7d5b1f);
        ctx.begin_path();
        ctx.move_to(-7.14f, -18.57f);
        ctx.line_to(7.14f, -18.57f);
        ctx.line_to(5.71f, 35.71f);
        ctx.line_to(-5.71f, 35.71f);
        ctx.close_path();
        ctx.fill();
        ctx.set_stroke(0xff654a19);
        ctx.set_line_width(2.14f);
        ctx.begin_path();
        ctx.move_to(-7.14f, -18.57f);
        ctx.line_to(7.14f, -18.57f);
        ctx.line_to(5.71f, 35.71f);
        ctx.line_to(-5.71f, 35.71f);
        ctx.close_path();
        ctx.stroke();

        // Head
        ctx.set_fill(0xff888888);
        ctx.begin_path();
        ctx.move_to(-27.143f, -5.714f);
        ctx.qcurve_to(-30.0f, -21.43f, -28.57f, -32.857f);
        ctx.qcurve_to(0.001f, -34.286f, 28.572f, -32.857f);
        ctx.qcurve_to(30.0f, -21.43f, 27.143f, -5.714f);
        ctx.close_path();
        ctx.fill();
        ctx.set_stroke(0xff6e6e6e);
        ctx.set_line_width(2.14f);
        ctx.begin_path();
        ctx.move_to(-27.143f, -5.714f);
        ctx.qcurve_to(-30.0f, -21.43f, -28.57f, -32.857f);
        ctx.qcurve_to(0.001f, -34.286f, 28.572f, -32.857f);
        ctx.qcurve_to(30.0f, -21.43f, 27.143f, -5.714f);
        ctx.close_path();
        ctx.stroke();

        // Dark disc behind the star
        ctx.set_fill(0xff6e6e6e);
        ctx.begin_path();
        ctx.arc(0, -20.0f, 14.286f);
        ctx.fill();

        // Cyan star centred at (0, -20)
        ctx.set_fill(0xff29f2e5);
        ctx.set_stroke(0xff21c4b9);
        ctx.set_line_width(1.2f);
        ctx.begin_path();
        ctx.move_to(5.00f, -20.00f);
        ctx.line_to(9.51f, -16.91f);
        ctx.line_to(4.05f, -17.06f);
        ctx.line_to(5.88f, -11.91f);
        ctx.line_to(1.55f, -15.25f);
        ctx.line_to(0.00f, -10.00f);
        ctx.line_to(-1.55f, -15.25f);
        ctx.line_to(-5.88f, -11.91f);
        ctx.line_to(-4.05f, -17.06f);
        ctx.line_to(-9.51f, -16.91f);
        ctx.line_to(-5.00f, -20.00f);
        ctx.line_to(-9.51f, -23.09f);
        ctx.line_to(-4.05f, -22.94f);
        ctx.line_to(-5.88f, -28.09f);
        ctx.line_to(-1.55f, -24.76f);
        ctx.line_to(0.00f, -30.00f);
        ctx.line_to(1.55f, -24.76f);
        ctx.line_to(5.88f, -28.09f);
        ctx.line_to(4.05f, -22.94f);
        ctx.line_to(9.51f, -23.09f);
        ctx.close_path();
        ctx.fill();
        ctx.stroke();
    }
}
