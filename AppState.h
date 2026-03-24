#pragma once
#include <algorithm>
#include <cfloat>
#include <string>
#include "Types.h"
#include "PitGenerator.h"   // PitParams (parametrik terrain için hâlâ mevcut)

// ── Lane'in kesim tenörü teorisi ──────────────────────────────────────────────
struct EconomicParams {
    float metalPrice   = 100.f;  // $/t metal
    float recovery     = 0.90f;  // 0–1
    float miningCost   = 30.f;   // $/t rock
    float processCost  = 20.f;   // $/t ore
    float sellingCost  = 5.f;    // $/t metal
    float marketCap    = 1e6f;   // limitleyici kapasite (isteğe bağlı)

    // Lane'in dengeleme formülü → optimal kesim tenörü (% metal).
    // Ekonomik olmayan parametre setinde FLT_MAX döndürür.
    float computeCutoffGrade() const {
        float netSmelter = metalPrice * recovery - miningCost;
        if (netSmelter <= 0.f) return FLT_MAX;
        float g_m = (processCost + sellingCost) / netSmelter;
        float g_p =  processCost                / netSmelter;
        float g_k = (processCost + sellingCost) / (metalPrice * recovery);
        float gc  = std::max({ std::min(g_m, g_p),
                               std::min(g_p, g_k),
                               std::min(g_m, g_k) });
        return std::max(0.f, gc);
    }
};

// ── Pit optimizasyon / kestirim parametreleri ─────────────────────────────────
//
//  slopeAngleDeg: Genel şev açısı (°), 20–85 arası mantıklı değerler.
//    LGOptimizer::build() bu değeri kullanarak her blok çifti için koni
//    bağımlılığını dinamik olarak hesaplar.  Formül:
//
//      Δy_world = (iy_parent – iy_child) * blockSize   [bench farkı, metre]
//      Δd_world = sqrt((Δix²+Δiz²)) * blockSize         [yatay mesafe, metre]
//      R        = tan(slopeAngleDeg) * Δy_world          [arama yarıçapı]
//      Bağımlılık varsa: Δd_world <= R
//
struct PitOptimizationParams {
    float            slopeAngleDeg  = 45.f;              // genel şev açısı
    EstimationMethod estimMethod    = EstimationMethod::IDW;
    int              idwPower       = 2;                  // IDW üssü (1–4)
    float            searchRadius   = 0.f;                // 0 → 6 × blockSize (otomatik)
    float            rockDensity    = 2.7f;               // t/m³
};

// ── Tek-atış olay bayrakları ──────────────────────────────────────────────────
//  GUI tarafından set edilir, SceneController::tick() veya main döngüsü
//  tarafından işlenip sıfırlanır.  Böylece SPACE'e bağlı doğrusal akış
//  tamamen ortadan kalkar; kullanıcı istediği zaman herhangi bir işlemi
//  başlatabilir.
struct AppEvents {
    bool runEstimation   = false;   // IDW / NN kestirimini başlat
    bool runOptimization = false;   // LG thread'ini başlat
    bool exportReport    = false;   // CSV raporu yaz
    bool saveProject     = false;   // Proje dosyasına kaydet (Faz 4)
    bool loadProject     = false;   // Proje dosyasından yükle (Faz 4)
    std::string projectPath;        // save/load için dosya yolu
};

// ── Uygulama durumu (global singleton) ───────────────────────────────────────
struct AppState {
    // ── Fare / giriş ──────────────────────────────────────────────────────────
    double lastX     = 0, lastY = 0;
    bool   leftDown  = false;
    bool   rightDown = false;

    // ── Sahne ─────────────────────────────────────────────────────────────────
    AppStage   stage = AppStage::EMPTY;
    ViewPreset view  = ViewPreset::FREE;

    // ── Parametreler ──────────────────────────────────────────────────────────
    PitParams             params;   // Parametrik terrain (başlangıç görseli kaldırıldı,
                                    //  ama struct varlığını koruyoruz — PitGenerator hâlâ kullanıyor)
    EconomicParams        econ;
    PitOptimizationParams opt;

    // ── Render bayrakları ──────────────────────────────────────────────────────
    RenderSettings render;

    // ── Olaylar (GUI → main loop / SceneController) ──────────────────────────
    AppEvents events;

    // ── Animasyon ─────────────────────────────────────────────────────────────
    bool animating   = false;   // PIT_SHELL'de bench simülasyonu, diğerlerinde auto-rotate
    bool needRegen   = false;   // Parametrik terrain yeniden oluşturulsun (şimdilik kullanılmıyor)
};
