#include <Client/Render/MapRenderer.hh>

#include <emscripten.h>

using namespace Ui;

void MapRenderer::load() {
    EM_ASM({
        Module.mapReady = false;
        fetch('map-data.json').then(function(r){ return r.json(); }).then(function(d){
            const tiles = {};
            for (const gid in d.tiles) {
                tiles[gid] = d.tiles[gid].map(function(s){
                    return { path: new Path2D(s.d), fill: s.fill, stroke: s.stroke, sw: s.sw };
                });
            }
            const NL = d.layers.length, GW = 50;
            const layerGrid = [];
            for (let i = 0; i < NL; i++) layerGrid.push(new Map());
            for (const p of d.placements) layerGrid[p.l].set(p.r * GW + p.c, p);
            Module.mapTiles = tiles;
            Module.mapLayers = layerGrid;
            Module.mapObjects = d.objects;
            Module.mapLayerCount = NL;
            Module.mapCell = d.cell;
            Module.mapTileSize = d.tileSize;
            Module.mapReady = true;
        });
    });
}

// draw() implemented in Task 3.
void MapRenderer::draw(int, int, int, int, int) {}
