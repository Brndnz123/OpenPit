#pragma once
#include <glm/glm.hpp>
#include <cstdint>

// ── Block durumu ──────────────────────────────────────────────────────────────
enum class LGState : uint8_t { ACTIVE, IN_PIT, DISCARDED, MINED };

// ── Blok renk modları (LITHOLOGY kaldırıldı) ──────────────────────────────────
enum class BlockColorMode { FLAT, GRADE, ECONOMIC };

// ── Uygulama aşaması — artık yalnızca bilgilendirme amaçlı,
//    geçişler event-driven mimaride SceneController::tick() içinde yönetilir ──
enum class AppStage {
    EMPTY,       // Veri yüklenmedi
    DRILLHOLES,  // Collar/sample yüklendi, sondaj yolları gösteriliyor
    ESTIMATED,   // Tenör kestirim tamamlandı
    OPTIMIZING,  // LG thread çalışıyor
    PIT_SHELL    // Optimizasyon bitti, pit kabuğu görünür
};

// ── Kamera ön-ayarları ────────────────────────────────────────────────────────
enum class ViewPreset { FREE, TOP, SIDE, SECTION };

// ── Kestirim yöntemleri ───────────────────────────────────────────────────────
enum class EstimationMethod { IDW, NEAREST_NEIGHBOR };

// ── Tek blok ──────────────────────────────────────────────────────────────────
//  Notlar:
//    • lithologyId ve slopeCells kaldırıldı — şev açısı artık
//      PitOptimizationParams::slopeAngleDeg üzerinden dinamik hesaplanıyor.
//    • worldPos: OpenGL koordinatları (X sağ, Y yukarı, Z ekrana doğru).
//      iy=0 en üst bench; Y değeri negatife doğru artar (derinleşir).
struct Block {
    int       id      = 0;
    int       ix      = 0, iy = 0, iz = 0;
    glm::vec3 worldPos{};
    float     grade   = 0.f;   // % metal (IDW veya NN ile tahmin edilmiş)
    float     value   = 0.f;   // ekonomik değer ($); eksi = waste
    LGState   state   = LGState::ACTIVE;
};

// ── Render ayarları ───────────────────────────────────────────────────────────
struct RenderSettings {
    bool  showWireframe  = false;
    bool  showBlocks     = false;
    bool  showDrillholes = false;
    bool  showGrid       = true;
    bool  showAxes       = true;
    float ambientStr     = 0.20f;
    float diffuseStr     = 0.80f;
};
