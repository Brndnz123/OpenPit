#include "LGOptimizer.h"
#include <queue>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <glm/glm.hpp>

// ── Dinic maksimum akış çekirdeği ─────────────────────────────────────────────

void LGOptimizer::addEdge(int u, int v, float cap) {
    _graph[u].push_back({ v, (int)_graph[v].size(), cap });
    _graph[v].push_back({ u, (int)_graph[u].size() - 1, 0.f });
}

bool LGOptimizer::bfs() {
    _level.assign(_numNodes, -1);
    std::queue<int> q;
    _level[_S] = 0; q.push(_S);
    while (!q.empty()) {
        int v = q.front(); q.pop();
        for (const auto& e : _graph[v])
            if (e.cap > 1e-7f && _level[e.to] < 0) {
                _level[e.to] = _level[v] + 1;
                q.push(e.to);
            }
    }
    return _level[_T] >= 0;
}

float LGOptimizer::dfs(int v, float pushed) {
    if (v == _T || pushed < 1e-7f) return pushed;
    for (int& i = _iter[v]; i < (int)_graph[v].size(); ++i) {
        Edge& e = _graph[v][i];
        if (e.cap < 1e-7f || _level[e.to] != _level[v] + 1) continue;
        float d = dfs(e.to, std::min(pushed, e.cap));
        if (d > 1e-7f) {
            e.cap -= d;
            _graph[e.to][e.rev].cap += d;
            return d;
        }
    }
    return 0.f;
}

// ── Dinamik koni bağımlılık grafiği inşası ────────────────────────────────────
//
//  Her (ix, iy, iz) bloğu için üstteki bench'lerdeki bloklara koni
//  koşuluyla bağımlılık kenarı eklenir.
//
//  Koni koşulu (iy_üst < iy_alt, çünkü iy=0 yüzey):
//    Δy    = (iy_alt – iy_üst) × blockSize            [metre, pozitif]
//    Δd    = sqrt((Δix)² + (Δiz)²) × blockSize         [metre]
//    R     = tan(α) × Δy                               [metre]
//    Bağımlılık varsa: Δd ≤ R
//
//  Sadece bir bench farkı (Δiy = 1) için arama genişliği hesaplamak
//  yeterlidir; birden fazla bench için kenar hem gereksiz hem de grafı
//  şişirir.  Tek bench arama yarıçapını blok cinsine çevirip tarama
//  döngüsü sınırını belirliyoruz.

void LGOptimizer::build(std::vector<Block>& blocks,
                        int nx, int ny, int nz,
                        float blockSize,
                        float slopeAngleDeg) {
    const int N       = (int)blocks.size();
    _numNodes = N + 2; _S = N; _T = N + 1;
    _graph.assign(_numNodes, {});
    _positiveSum = 0.0;

    // Kenar bağlantı dizisi: id = ix + iz*nx + iy*nx*nz
    auto idx = [&](int bx, int by, int bz) -> int {
        return bx + bz * nx + by * nx * nz;
    };

    // INF kapasitesi: toplam pozitif değerin üstünde bir sayı
    float INF = 0.f;
    for (const auto& b : blocks) if (b.value > 0.f) INF += b.value;
    INF = std::max(INF, 1.f) * 10.f;

    // Koni parametreleri — tek bench (Δiy=1) için yatay arama yarıçapı (blok)
    const double alphaRad  = slopeAngleDeg * (3.14159265358979323846 / 180.0);
    const double tanAlpha  = std::tan(alphaRad);
    // R_metre = tan(α) × blockSize (tek bench farkı için)
    // R_blok  = R_metre / blockSize = tan(α)
    // Tarama döngüsü için tavan: ceil(tanAlpha) + 1 (tam sayıya yuvarla)
    const int scanRadius = (int)std::ceil(tanAlpha) + 1;

    long edgeCount = 0, depCount = 0;

    for (int i = 0; i < N; ++i) {
        const Block& b = blocks[i];

        // S-T bağlantısı
        if (b.value > 0.f) {
            addEdge(_S, b.id, b.value);
            _positiveSum += b.value;
        } else {
            addEdge(b.id, _T, -b.value);
        }
        ++edgeCount;

        // Üst bench'lere koni bağımlılığı:
        // "b kazılabilmek için üstündeki blok kazılmış olmalı"
        if (b.iy == 0) continue;  // en üst bench'in üstü yok

        // Sadece bir bench yukarı bak (Δiy = 1) — transitif kapatma grafı
        // zaten tüm zinciri kapsar; birden fazla bench atlamak gereksizdir.
        const int iy_upper = b.iy - 1;

        for (int dix = -scanRadius; dix <= scanRadius; ++dix) {
            for (int diz = -scanRadius; diz <= scanRadius; ++diz) {
                // Kesin koni kosulu (blok merkezi tabanli)
                double deltaD = std::sqrt((double)(dix * dix + diz * diz)) * blockSize;
                double R = tanAlpha * blockSize;
                if (deltaD > R + 1e-6) continue;

                int ax = b.ix + dix;
                int az = b.iz + diz;
                if (ax < 0 || ax >= nx || az < 0 || az >= nz) continue;

                addEdge(b.id, idx(ax, iy_upper, az), INF);
                ++edgeCount; ++depCount;
            }
        }
    }

    std::cout << "[LG] Dugum: " << _numNodes
        << "  Kenar: " << edgeCount
        << "  Bagimlilik: " << depCount
        << "  (alpha=" << slopeAngleDeg << " derece"
        << "  scanR=" << scanRadius << " blok)\n"
        << "[LG] Pozitif toplam = " << _positiveSum << "\n";
}

double LGOptimizer::solve() {
    auto t0 = std::chrono::high_resolution_clock::now();
    _maxFlow = 0.0;
    while (bfs()) {
        _iter.assign(_numNodes, 0);
        while (true) {
            float f = dfs(_S, (float)_positiveSum * 10.f);
            if (f < 1e-7f) break;
            _maxFlow += f;
        }
    }
    double ms = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - t0).count();
    std::cout << "[LG] Akış=" << _maxFlow
              << "  Optimal=" << optimalValue()
              << "  Süre=" << ms << "ms\n";
    return _maxFlow;
}

void LGOptimizer::applyResult(std::vector<Block>& blocks) {
    std::vector<bool> reach(_numNodes, false);
    std::queue<int> q;
    q.push(_S); reach[_S] = true;
    while (!q.empty()) {
        int v = q.front(); q.pop();
        for (const auto& e : _graph[v])
            if (!reach[e.to] && e.cap > 1e-7f) {
                reach[e.to] = true;
                q.push(e.to);
            }
    }
    int nIn = 0, nOut = 0;
    for (auto& b : blocks) {
        if (reach[b.id]) { b.state = LGState::IN_PIT;    ++nIn;  }
        else             { b.state = LGState::DISCARDED;  ++nOut; }
    }
    std::cout << "[LG] In-pit: " << nIn << "  Discarded: " << nOut << "\n";
}

void LGOptimizer::optimize(std::vector<Block>& blocks,
                            int nx, int ny, int nz,
                            float blockSize,
                            float slopeAngleDeg) {
    build(blocks, nx, ny, nz, blockSize, slopeAngleDeg);
    solve();
    applyResult(blocks);
}
