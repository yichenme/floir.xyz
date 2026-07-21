#pragma once

namespace Ui {
    // Loads Server/map-data.json and draws the map as cached vector tiles.
    struct MapRenderer {
        static void load();
        // Draw visible tiles/objects. Cell range is [c0,c1) x [r0,r1); the ctx
        // (id) is already under the camera (world) transform.
        static void draw(int ctx_id, int c0, int c1, int r0, int r1);
        // Redraw a single named layer (by index into map-data.json's `layers`)
        // over a small cell range, on top of whatever was already drawn there.
        // Used to paint the tunnel (暗道) layer back over a "hidden" player after
        // its flower renders, so it visually reads as tucked under the tunnel.
        static void draw_layer(int ctx_id, int layer, int c0, int c1, int r0, int r1);
    };
}
