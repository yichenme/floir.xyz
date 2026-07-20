#include <Client/Assets/Assets.hh>

#include <Client/StaticData.hh>

#include <Helpers/Bits.hh>
#include <Helpers/Math.hh>

#include <cmath>

#define SET_BASE_COLOR(set_color) { if (!BitMath::at(flags, 0)) base_color = set_color; else { base_color = FLOWER_COLORS[attr.color]; } }

void draw_static_mob(MobID::T mob_id, Renderer &ctx, MobRenderAttributes attr) {
    float radius = attr.radius;
    uint32_t flags = attr.flags;
    float animation_value = sinf(attr.animation);
    uint32_t seed = attr.seed;
    uint32_t base_color = 0xffffe763;
    switch(mob_id) {
        case MobID::kBabyAnt:
            SET_BASE_COLOR(0xff555555)
            // Scale the whole model by rarity size so the mandibles keep the
            // same insertion depth into the body as at base (common) size.
            ctx.scale(radius / 14.0f);
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(0, -7);
            ctx.qcurve_to(11, -10 + animation_value, 22, -5 + animation_value);
            ctx.move_to(0, 7);
            ctx.qcurve_to(11, 10 - animation_value, 22, 5 - animation_value);
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.arc(0,0,14);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kWorkerAnt:
            SET_BASE_COLOR(0xff555555)
            // Uniform rarity scaling so head + mandibles keep their base ratio.
            ctx.scale(radius / 14.0f);
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.begin_path();
            ctx.arc(-12, 0, 10);
            ctx.fill();
            ctx.stroke();
            ctx.set_stroke(0xff292929);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(4, -7);
            ctx.qcurve_to(15, -10 + animation_value, 26, -5 + animation_value);
            ctx.move_to(4, 7);
            ctx.qcurve_to(15, 10 - animation_value, 26, 5 - animation_value);
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.arc(4,0,14);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kSoldierAnt:
            SET_BASE_COLOR(0xff555555)
            // Uniform rarity scaling so head, wing-plates + mandibles keep ratio.
            ctx.scale(radius / 14.0f);
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.begin_path();
            ctx.arc(-12, 0, 10);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(0x80eeeeee);
            {
                RenderContext context(&ctx);
                ctx.begin_path();
                ctx.rotate(0.1 * animation_value);
                ctx.translate(-11, -8);
                ctx.rotate(0.1 * M_PI);
                ctx.ellipse(0,0,15,7);
                ctx.fill();
            }
            {
                RenderContext context(&ctx);
                ctx.begin_path();
                ctx.rotate(-0.1 * animation_value);
                ctx.translate(-11, 8);
                ctx.rotate(-0.1 * M_PI);
                ctx.ellipse(0,0,15,7);
                ctx.fill();
            }
            ctx.set_stroke(0xff292929);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(4, -7);
            ctx.qcurve_to(15, -10 + animation_value, 26, -5 + animation_value);
            ctx.move_to(4, 7);
            ctx.qcurve_to(15, 10 - animation_value, 26, 5 - animation_value);
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.arc(4,0,14);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kBee:
            SET_BASE_COLOR(0xffffe763)
            // Scale the model to the hitbox so it grows with rarity instead of
            // staying a fixed size inside an enlarged collision circle.
            ctx.scale(radius / 20.0f);
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(-25,9);
            ctx.line_to(-37,0);
            ctx.line_to(-25,-9);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.begin_path();
            ctx.ellipse(0,0,30,20);
            ctx.fill();
            {
                RenderContext context(&ctx);
                ctx.clip();
                ctx.set_fill(0xff333333);
                ctx.fill_rect(-30,-20,10,40);
                ctx.fill_rect(-10,-20,10,40);
                ctx.fill_rect(10,-20,10,40);
            }
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.begin_path();
            ctx.ellipse(0,0,30,20);
            ctx.stroke();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(25,-5);
            ctx.qcurve_to(35,-5,40,-15);
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.begin_path();
            ctx.arc(40,-15,5);
            ctx.fill();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(25,5);
            ctx.qcurve_to(35,5,40,15);
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.begin_path();
            ctx.arc(40,15,5);
            ctx.fill();
            break;
        case MobID::kLadybug:
        case MobID::kDarkLadybug:
        case MobID::kShinyLadybug:
            ctx.scale(radius / 30);
            if (mob_id == MobID::kDarkLadybug) SET_BASE_COLOR(0xff962921)
            else if (mob_id == MobID::kShinyLadybug) SET_BASE_COLOR(0xffebeb34)
            else SET_BASE_COLOR(0xffeb4034)
            ctx.set_fill(0xff111111);
            ctx.begin_path();
            ctx.arc(15,0,18.5);
            ctx.fill();
            ctx.set_fill(base_color);
            ctx.begin_path();
            ctx.move_to(24.760068893432617,16.939273834228516);
            ctx.qcurve_to(17.74359130859375,27.195226669311523,5.530136585235596,29.485883712768555);
            ctx.qcurve_to(-6.683317184448242,31.77654457092285,-16.939273834228516,24.760068893432617);
            ctx.qcurve_to(-27.195226669311523,17.74359130859375,-29.485883712768555,5.530136585235596);
            ctx.qcurve_to(-31.77654457092285,-6.683317184448242,-24.760068893432617,-16.939273834228516);
            ctx.qcurve_to(-17.74359130859375,-27.195226669311523,-5.530136585235596,-29.485883712768555);
            ctx.qcurve_to(6.683317184448242,-31.77654457092285,16.939273834228516,-24.760068893432617);
            ctx.qcurve_to(19.241104125976562,-23.185302734375,21.213199615478516,-21.213205337524414);
            ctx.qcurve_to(23.1852970123291,-19.241111755371094,24.76006507873535,-16.939281463623047);
            ctx.qcurve_to(10,0,24.760068893432617,16.939273834228516);
            ctx.fill(1);
            {
                RenderContext context(&ctx);
                ctx.clip();
                if (mob_id == MobID::kDarkLadybug) ctx.set_fill(Renderer::HSV(base_color, 1.2));
                else ctx.set_fill(0xff111111);
                SeedGenerator gen(seed * 374572 + 46237);
                uint32_t ct = 1 + gen.next() * 7;
                for (uint32_t i = 0; i < ct; ++i) {
                    ctx.begin_path();
                    ctx.arc(gen.binext()*30,gen.binext()*30,4+gen.next()*5);
                    ctx.fill();
                }
            }
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.move_to(27.64874267578125,18.915523529052734);
            ctx.qcurve_to(19.813682556152344,30.36800765991211,6.175320625305176,32.925907135009766);
            ctx.qcurve_to(-7.463029861450195,35.48381042480469,-18.91551971435547,27.648746490478516);
            ctx.qcurve_to(-30.36800765991211,19.813682556152344,-32.925907135009766,6.175320625305176);
            ctx.qcurve_to(-35.48381042480469,-7.463029861450195,-27.648746490478516,-18.91551971435547);
            ctx.qcurve_to(-19.813682556152344,-30.36800765991211,-6.175320625305176,-32.925907135009766);
            ctx.qcurve_to(7.463029861450195,-35.48381042480469,18.91551971435547,-27.648746490478516);
            ctx.qcurve_to(24.10110092163086,-24.101102828979492,27.648740768432617,-18.915529251098633);
            ctx.qcurve_to(28.323867797851562,-17.928699493408203,28.25410270690918,-16.73506736755371);
            ctx.qcurve_to(28.184337615966797,-15.541434288024902,27.398849487304688,-14.639973640441895);
            ctx.qcurve_to(14.642288208007812,0,27.398853302001953,14.639965057373047);
            ctx.qcurve_to(28.184343338012695,15.541427612304688,28.254106521606445,16.735061645507812);
            ctx.qcurve_to(28.323869705200195,17.928693771362305,27.64874267578125,18.9155216217041);
            ctx.line_to(27.64874267578125,18.915523529052734);
            ctx.move_to(21.871395111083984,14.963025093078613);
            ctx.line_to(24.760068893432617,16.939273834228516);
            ctx.line_to(22.12128448486328,19.238582611083984);
            ctx.qcurve_to(5.3577117919921875,0,22.121280670166016,-19.238590240478516);
            ctx.line_to(24.76006507873535,-16.939281463623047);
            ctx.line_to(21.871389389038086,-14.963033676147461);
            ctx.qcurve_to(19.065046310424805,-19.0650577545166,14.96302318572998,-21.871395111083984);
            ctx.qcurve_to(5.903592586517334,-28.06928253173828,-4.884955406188965,-26.045866012573242);
            ctx.qcurve_to(-15.673511505126953,-24.022449493408203,-21.871395111083984,-14.96302318572998);
            ctx.qcurve_to(-28.06928253173828,-5.903592586517334,-26.045866012573242,4.884955406188965);
            ctx.qcurve_to(-24.022449493408203,15.673511505126953,-14.96302318572998,21.871395111083984);
            ctx.qcurve_to(-5.903592586517334,28.06928253173828,4.884955406188965,26.045866012573242);
            ctx.qcurve_to(15.673511505126953,24.022449493408203,21.871395111083984,14.963025093078613);
            ctx.fill(1);
            break;
        case MobID::kBeetle:
            ctx.scale(radius / 35);
            SET_BASE_COLOR(0xff905db0)
            ctx.begin_path();
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.translate(35,0);
            {
                RenderContext context(&ctx);
                ctx.rotate(-0.1 * animation_value);
                ctx.move_to(-10,15);
                ctx.qcurve_to(15,30,35,15);
                ctx.qcurve_to(15,20,-10,5);
                ctx.line_to(-10,15);
                ctx.fill();
                ctx.stroke();
            }
            {
                RenderContext context(&ctx);
                ctx.rotate(0.1 * animation_value);
                ctx.move_to(-10,-15);
                ctx.qcurve_to(15,-30,35,-15);
                ctx.qcurve_to(15,-20,-10,-5);
                ctx.line_to(-10,-15);
                ctx.fill();
                ctx.stroke();
            }
            ctx.translate(-35,0);
            ctx.begin_path();
            ctx.move_to(0,-30);
            ctx.qcurve_to(40,-30,40,0);
            ctx.qcurve_to(40,30,0,30);
            ctx.qcurve_to(-40,30,-40,0);
            ctx.qcurve_to(-40,-30,0,-30);
            ctx.set_fill(base_color);
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(0,-33.5);
            ctx.qcurve_to(43.5,-33.5,43.5,0);
            ctx.qcurve_to(43.5,33.5,0,33.5);
            ctx.qcurve_to(-43.5,33.5,-43.5,0);
            ctx.qcurve_to(-43.5,-33.5,0,-33.5);
            ctx.move_to(0,-26.5);
            ctx.qcurve_to(-36.5,-26.5,-36.5,0);
            ctx.qcurve_to(-36.5,26.5,0,26.5);
            ctx.qcurve_to(36.5,26.5,36.5,0);
            ctx.qcurve_to(36.5,-26.5,0,-26.5);
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(-20,0);
            ctx.qcurve_to(0,-3,20,0);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.stroke();
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.move_to(-17,-12);
            ctx.arc(-17,-12,5);
            ctx.move_to(-17,-2);
            ctx.arc(-17,12,5);
            ctx.move_to(0,-15);
            ctx.arc(0,-15,5);
            ctx.move_to(0,15);
            ctx.arc(0,15,5);
            ctx.move_to(17,-12);
            ctx.arc(17,-12,5);
            ctx.move_to(17,12);
            ctx.arc(17,12,5);
            ctx.fill();
            break;
        case MobID::kHornet:
            SET_BASE_COLOR(0xffffe763)
            // Scale the model to the hitbox so it tracks the collision circle
            // at every rarity instead of staying a fixed size.
            ctx.scale(radius / 40.0f);
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(-25,-6);
            ctx.line_to(-47,0);
            ctx.line_to(-25,6);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.begin_path();
            ctx.ellipse(0,0,30,20);
            ctx.fill();
            {
                RenderContext context(&ctx);
                ctx.clip();
                ctx.set_fill(0xff333333);
                ctx.fill_rect(-30,-20,10,40);
                ctx.fill_rect(-10,-20,10,40);
                ctx.fill_rect(10,-20,10,40);
            }
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.begin_path();
            ctx.ellipse(0,0,30,20);
            ctx.stroke();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(25, 5);
            ctx.qcurve_to(40, 10, 50, 15);
            ctx.qcurve_to(40, 5, 25, 5);
            ctx.move_to(25, -5);
            ctx.qcurve_to(40, -10, 50, -15);
            ctx.qcurve_to(40, -5, 25, -5);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kCactus: {
            SET_BASE_COLOR(0xff32a852)
            uint32_t vertices = radius / 10 + 5;
            {
                RenderContext context(&ctx);
                ctx.set_fill(0xff222222);
                ctx.begin_path();
                for (uint32_t i = 0; i < vertices; ++i) {
                    ctx.move_to(10+radius,0);
                    ctx.line_to(0.5+radius,3);
                    ctx.line_to(0.5+radius,-3);
                    ctx.line_to(10+radius,0);
                    ctx.rotate(M_PI * 2 / vertices);
                }
                ctx.fill();
            }
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(radius,0);
            for (uint32_t i = 0; i < vertices; ++i) {
                float base_angle = M_PI * 2 * i / vertices;
                ctx.qcurve_to(
                    radius*0.8*cosf(base_angle+M_PI/vertices),radius*0.8*sinf(base_angle+M_PI/vertices),
                    radius*cosf(base_angle+2*M_PI/vertices),radius*sinf(base_angle+2*M_PI/vertices));
            }
            ctx.fill();
            ctx.stroke();
            break;
        }
        case MobID::kRock:
        {
            SET_BASE_COLOR(0xff777777)
            SeedGenerator gen(std::floor(radius) * 1957264 + 295726);
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            // 0.1 read as barely more than a plain circle at gallery-icon
            // scale (indistinguishable from the Rock petal's own grey disc);
            // 0.25 keeps a visibly jagged, angular boulder silhouette down to
            // small sizes.
            float deflection = radius * 0.25;
            ctx.move_to(radius + gen.binext() * deflection,gen.binext() * deflection);
            uint32_t sides = 4 + radius / 10;
            for (uint32_t i = 1; i < sides; ++i) {
                float angle = 2 * M_PI * i / sides;
                ctx.line_to(cosf(angle) * radius + gen.binext() * deflection, sinf(angle) * radius + gen.binext() * deflection);
            }
            ctx.close_path();
            ctx.fill();
            ctx.stroke();
            break;
        }
        case MobID::kCentipede:
        case MobID::kEvilCentipede:
        case MobID::kDesertCentipede:
            if (mob_id == MobID::kCentipede) SET_BASE_COLOR(0xff8ac255)
            else if (mob_id == MobID::kEvilCentipede) SET_BASE_COLOR(0xff905db0)
            else SET_BASE_COLOR(0xffd4c66e)
            // Scale each segment to its hitbox so the body circle matches the
            // collision radius (and segments stay connected) at every rarity.
            ctx.scale(radius / 35.0f);
            ctx.set_fill(0xff333333);
            ctx.begin_path();
            ctx.arc(0,-30,15);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,30,15);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,35);
            ctx.set_fill(base_color);
            ctx.fill();
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.stroke();
            if (!BitMath::at(flags, 1)) {
                ctx.begin_path();
                ctx.move_to(25,-10);
                ctx.qcurve_to(45,-10,55,-30);
                ctx.set_stroke(0xff333333);
                ctx.set_line_width(3);
                ctx.stroke();
                ctx.begin_path();
                ctx.arc(55,-30,5);
                ctx.set_fill(0xff333333);
                ctx.fill();
                ctx.begin_path();
                ctx.move_to(25,10);
                ctx.qcurve_to(45,10,55,30);
                ctx.stroke();
                ctx.begin_path();
                ctx.arc(55,30,5);
                ctx.fill();
            }
            break;
        case MobID::kSpider:
            SET_BASE_COLOR(0xff4f412e);
            // Scale the whole model by rarity size so the legs keep the same
            // length + body-insertion ratio as at base (common) size.
            ctx.scale(radius / 15.0f);
            ctx.set_fill(base_color);
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.begin_path();
            #define draw_leg(angle) \
            { \
                float cos = cosf(angle) * 35; \
                float sin = sinf(angle) * 35; \
                ctx.move_to(0,0); \
                ctx.qcurve_to(sin * 0.8, cos * 0.5, sin, cos); \
            }
            draw_leg(-M_PI + 0.9 + sinf(attr.animation) * 0.2)
            draw_leg(-M_PI + 0.3 + cosf(attr.animation) * 0.2)
            draw_leg(-M_PI - 0.3 + sinf(attr.animation) * 0.2)
            draw_leg(-M_PI - 0.9 - cosf(attr.animation) * 0.2)
            draw_leg(-0.9 - sinf(attr.animation) * 0.2)
            draw_leg(-0.3 + cosf(attr.animation) * 0.2)
            draw_leg(0.3 - sinf(attr.animation) * 0.2)
            draw_leg(0.9 - cosf(attr.animation) * 0.2)
            #undef draw_leg
            ctx.stroke();
            ctx.begin_path();
            ctx.arc(0,0,15);
            ctx.fill();
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.stroke();
            break;
        case MobID::kSandstorm:
            SET_BASE_COLOR(0xffd5c7a6)
            ctx.set_line_width(radius / 5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.set_fill(base_color);
            ctx.set_stroke(base_color);
            ctx.rotate(attr.animation / 3);
            ctx.begin_path();
            ctx.move_to(radius, 0);
            for (uint32_t i = 1; i <= 6; ++i) {
                float angle = 2 * M_PI * i / 6;
                ctx.line_to(cosf(angle) * radius, sinf(angle) * radius);
            }
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(Renderer::HSV(base_color, 0.9));
            ctx.set_stroke(Renderer::HSV(base_color, 0.9));
            ctx.rotate(attr.animation / 3);
            ctx.begin_path();
            ctx.move_to(radius*2/3, 0);
            for (uint32_t i = 1; i <= 6; ++i) {
                float angle = 2 * M_PI * i / 6;
                ctx.line_to(cosf(angle) * radius*2/3, sinf(angle) * radius*2/3);
            }
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.rotate(attr.animation / 3);
            ctx.begin_path();
            ctx.move_to(radius/3, 0);
            for (uint32_t i = 1; i <= 6; ++i) {
                float angle = 2 * M_PI * i / 6;
                ctx.line_to(cosf(angle) * radius/3, sinf(angle) * radius/3);
            }
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kScorpion:
            ctx.scale(radius / 35);
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.round_line_join();
            {
                RenderContext context(&ctx);
                ctx.rotate(-0.05 * animation_value);
                ctx.begin_path();
                ctx.move_to(5,10.5);
                ctx.qcurve_to(30,21.5,50,10.5);
                ctx.qcurve_to(30,14,5,3.5);
                ctx.close_path();
            }
            {
                RenderContext context(&ctx);
                ctx.rotate(0.05 * animation_value);
                ctx.move_to(5,-10.5);
                ctx.qcurve_to(30,-21.5,50,-10.5);
                ctx.qcurve_to(30,-14,5,-3.5);
                ctx.close_path();
            }
            ctx.fill();
            ctx.stroke();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.begin_path();
            #define draw_leg(angle) \
            { \
                float cos = cosf(angle) * 37; \
                float sin = sinf(angle) * 37; \
                ctx.move_to(0,0); \
                ctx.qcurve_to(sin * 0.7, cos * 0.5, sin, cos); \
            }
            draw_leg(-M_PI + 0.7 + sinf(attr.animation) * 0.15)
            draw_leg(-M_PI + 0.233 + cosf(attr.animation) * 0.15)
            draw_leg(-M_PI - 0.233 + sinf(attr.animation) * 0.15)
            draw_leg(-M_PI - 0.7 - cosf(attr.animation) * 0.15)
            draw_leg(-0.7 - sinf(attr.animation) * 0.15)
            draw_leg(-0.233 + cosf(attr.animation) * 0.15)
            draw_leg(0.233 - sinf(attr.animation) * 0.15)
            draw_leg(0.7 - cosf(attr.animation) * 0.15)
            ctx.stroke();
            SET_BASE_COLOR(0xffc69a2d);
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.move_to(0,-30);
            ctx.qcurve_to(40,-20,40,0);
            ctx.qcurve_to(40,20,0,30);
            ctx.qcurve_to(-40,35,-40,0);
            ctx.qcurve_to(-40,-35,0,-30);
            ctx.fill();
            ctx.stroke();
            ctx.set_line_width(7);
            ctx.begin_path();
            ctx.move_to(22,-12);
            ctx.qcurve_to(26,0,22,12);
            ctx.move_to(7,-18);
            ctx.qcurve_to(10.5,0,7,18);
            ctx.move_to(-7,-18);
            ctx.qcurve_to(-10.5,0,-7,18);
            ctx.move_to(-22,-15);
            ctx.qcurve_to(-27,0,-22,15);
            ctx.stroke();
            ctx.set_line_width(5);
            ctx.begin_path();
            ctx.move_to(-45, 0);
            ctx.bcurve_to(-44.9098, 9.5, -41.6136, 14.25, -32.4196, 14.2);
            ctx.bcurve_to(-23.2258, 14.15, -12.0197, 9, -8.2491, 0);
            ctx.bcurve_to(-12.0197, -9, -23.2258, -14.15, -32.4196, -14.2);
            ctx.bcurve_to(-41.6136, -14.25, -44.9098, -9.5, -45, 0);
            ctx.close_path();
            ctx.fill();
            ctx.stroke();
            ctx.begin_path();
            ctx.move_to(-36,-5);
            ctx.qcurve_to(-37,0,-36,5);
            ctx.move_to(-25,5);
            ctx.qcurve_to(-27,0,-25,-5);
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff222222);
            ctx.begin_path();
            ctx.move_to(-5.7491, 0);
            ctx.line_to(-12.7491, -7);
            ctx.line_to(-12.7491, 7);
            ctx.line_to(-5.7491, 0);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kAntHole:
            SET_BASE_COLOR(0xffb58500);
            ctx.begin_path();
            ctx.arc(0,0,radius);
            ctx.set_fill(base_color);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,radius*2/3);
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,radius/3);
            ctx.set_fill(Renderer::HSV(base_color, 0.6));
            ctx.fill();
            break;
        case MobID::kQueenAnt:
            // Art was drawn at absolute coords with no radius scaling, so it
            // stayed a fixed size while the hitbox grows with radius/rarity.
            // Scale it to the hitbox (base radius is 50 -> natural size there).
            ctx.scale(radius / 50.0f);
            ctx.begin_path();
            ctx.arc(-25,0,33.5);
            ctx.set_fill(0xff454545);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(-25,0,26.5);
            ctx.set_fill(0xff555555);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,28.5);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,21.5);
            ctx.fill();
            {
                RenderContext context(&ctx);
                ctx.rotate(animation_value * 0.1);
                ctx.begin_path();
                ctx.ellipse(-14,-16,30,14,M_PI/10);
                ctx.set_fill(0x7feeeeee);
                ctx.fill();
            }
            {          
                RenderContext context(&ctx);
                ctx.rotate(-animation_value * 0.1);
                ctx.begin_path();
                ctx.ellipse(-14,16,30,14,-M_PI/10);
                ctx.set_fill(0x7feeeeee);
                ctx.fill();
            }
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(25,-10.5);
            ctx.qcurve_to(41.5,-15+2*animation_value,58,-7.5+2*animation_value);
            ctx.move_to(25,10.5);
            ctx.qcurve_to(41.5,15-2*animation_value,58,7.5-2*animation_value);
            ctx.stroke();
            ctx.begin_path();
            ctx.arc(25,0,24.5);
            ctx.set_fill(0xff454545);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(25,0,17.5);
            ctx.set_fill(0xff555555);
            ctx.fill();
            break;
        case MobID::kSquare:
            ctx.set_fill(0xffffe869);
            ctx.set_stroke(0xffcfbc55);
            ctx.set_line_width(0.15*radius);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.rect(-radius * 0.707, -radius * 0.707, radius * 1.414, radius * 1.414);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kDigger: {
            attr.flower_attrs.radius = attr.radius;
            BitMath::set(attr.flower_attrs.equip_flags, EquipmentFlags::kCutter);
            BitMath::set(attr.flower_attrs.face_flags, FaceFlags::kSquareEyes);
            draw_static_flower(ctx, attr.flower_attrs);
            break;
        };
        case MobID::kMoon: {
            // Grey cratered moon rock (same look as the old Moon petal).
            ctx.set_fill(0xff878787);
            ctx.set_stroke(0xff6d6d6d);
            ctx.set_line_width(radius * 0.1f);
            ctx.begin_path();
            ctx.arc(0, 0, radius);
            ctx.stroke();
            ctx.fill();
            ctx.clip();
            SeedGenerator gen(seed * 274633 + 284562);
            ctx.set_fill(0xff999999);
            ctx.set_stroke(0xff7c7c7c);
            ctx.set_line_width(radius * 0.06f);
            ctx.begin_path();
            for (uint32_t i = 0; i < 10; ++i) {
                float _x = gen.binext() * radius;
                float _y = gen.binext() * radius;
                float _r = (gen.binext() * 0.5f + 0.5f) * radius * 0.22f;
                ctx.move_to(_x, _y);
                ctx.arc(_x, _y, _r);
            }
            ctx.stroke();
            ctx.fill(1);
            break;
        }
        case MobID::kBumbleBee: {
            // A rounder, fuzzier bee. Same construction as kBee (body ellipse +
            // black stripes clipped in, wing, antennae), tuned a touch chunkier.
            SET_BASE_COLOR(0xffffd363)
            ctx.scale(radius / 22.0f);
            // Stinger tail.
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(-26, 10);
            ctx.line_to(-40, 0);
            ctx.line_to(-26, -10);
            ctx.fill();
            ctx.stroke();
            // Body.
            ctx.set_fill(base_color);
            ctx.begin_path();
            ctx.ellipse(0, 0, 30, 22);
            ctx.fill();
            {
                RenderContext context(&ctx);
                ctx.clip();
                ctx.set_fill(0xff333333);
                ctx.fill_rect(-32, -22, 11, 44);
                ctx.fill_rect(-11, -22, 11, 44);
                ctx.fill_rect(10, -22, 11, 44);
            }
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.begin_path();
            ctx.ellipse(0, 0, 30, 22);
            ctx.stroke();
            // Antennae.
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(25, -6);
            ctx.qcurve_to(36, -6, 42, -16);
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.begin_path();
            ctx.arc(42, -16, 5);
            ctx.fill();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(25, 6);
            ctx.qcurve_to(36, 6, 42, 16);
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.begin_path();
            ctx.arc(42, 16, 5);
            ctx.fill();
            break;
        }
        case MobID::kDandelion: {
            // Literal trace of petals-and-mobs/mobs/36.svg -- the actual
            // Dandelion MOB asset (a body ringed by 10 stalks each ending in
            // a small seed), not the petal's puff-and-seed-mark design.
            // Precise 10-fold symmetric shape measured from the real path
            // data: body grey/white r=17.67/15.67 at center, 10 stalks from
            // radius ~16.66 to 30.0, 10 seeds at radius 30.0 with
            // grey/white r=7.0/5.0. Scaled by radius/37.0 so the seeds'
            // outer edge -- the shape's farthest point -- sits at the
            // physics radius.
            RenderContext context(&ctx);
            ctx.scale(radius / 37.0f);
            uint32_t const N = 10;
            float const BODY_OUTER = 17.67f, BODY_INNER = 15.67f;
            float const SEED_DIST = 30.0f, SEED_OUTER = 7.0f, SEED_INNER = 5.0f;
            float const STALK_START = 16.66f;
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(4.667f);
            ctx.round_line_cap();
            for (uint32_t i = 0; i < N; ++i) {
                float const a = 2 * M_PI * i / N;
                ctx.begin_path();
                ctx.move_to(cosf(a) * STALK_START, sinf(a) * STALK_START);
                ctx.line_to(cosf(a) * SEED_DIST, sinf(a) * SEED_DIST);
                ctx.stroke();
            }
            for (uint32_t i = 0; i < N; ++i) {
                float const a = 2 * M_PI * i / N;
                float const sx = cosf(a) * SEED_DIST, sy = sinf(a) * SEED_DIST;
                ctx.set_fill(0xffcfcfcf);
                ctx.begin_path();
                ctx.arc(sx, sy, SEED_OUTER);
                ctx.fill();
                ctx.set_fill(0xffffffff);
                ctx.begin_path();
                ctx.arc(sx, sy, SEED_INNER);
                ctx.fill();
            }
            ctx.set_fill(0xffcfcfcf);
            ctx.begin_path();
            ctx.arc(0, 0, BODY_OUTER);
            ctx.fill();
            ctx.set_fill(0xffffffff);
            ctx.begin_path();
            ctx.arc(0, 0, BODY_INNER);
            ctx.fill();
            break;
        }
        case MobID::kPollen: {
            // Identical to the Pollen petal (Client/Assets/Petal.cc, kPollen):
            // two concentric filled discs, darker gold ring + lighter yellow
            // core, same 0.647 inner/outer radius ratio.
            ctx.set_fill(0xffcfbb50);
            ctx.begin_path();
            ctx.arc(0, 0, radius);
            ctx.fill();
            ctx.set_fill(0xffffe763);
            ctx.begin_path();
            ctx.arc(0, 0, radius * 0.647f);
            ctx.fill();
            break;
        }
        default:
            assert(!"Didn't cover mob render");
            break;
    }
}