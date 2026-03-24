#include "SceneController.h"
#include "Renderer.h"
#include "Camera.h"
#include "PitGenerator.h"
#include "BlockModel.h"
#include "DrillholeDatabase.h"
#include "LGOptimizer.h"
#include <iostream>
#include <cmath>

SceneController::~SceneController() {
    if (_lgThread.joinable()) _lgThread.join();
}

void SceneController::setup(Renderer* r, Camera* c, PitGenerator* g,
                             BlockModel* bm, DrillholeDatabase* db,
                             LGOptimizer* opt) {
    _renderer = r; _camera = c; _gen = g;
    _bm = bm; _db = db; _opt = opt;
    _stage = AppStage::EMPTY;
    _dataReady = false;
}

// ── Veri yüklendikten sonra ───────────────────────────────────────────────────

void SceneController::onDataLoaded(AppState& state) {
    if (_db->getHoles().empty()) {
        std::cout << "[Scene] Sondaj yok — collar ve sample dosyaları gerekli.\n";
        return;
    }

    // ── Dinamik blok model boyutlandırması ────────────────────────────────────
    //  Collar koordinatlarından bounding box hesapla, blok boyutunu ve grid
    //  boyutunu otomatik ayarla.
    auto bb = _db->computeCollarBoundingBox();
    if (bb.valid) {
        const float bs = _bm->blockSize();  // mevcut blok boyutu (metre)

        // Alan genişlikleri + %20 kenar boşluğu
        double extX = (bb.maxX - bb.minX) * 1.20;
        double extZ = (bb.maxZ - bb.minZ) * 1.20;   // sondaj Y → OpenGL Z
        // Derinlik: en az 1 bench, maksimum sondaj derinliğine %30 eklentili
        double elevRange = (bb.maxY - bb.minY) * 1.30;

        int nx = std::max(10, (int)std::ceil(extX / bs));
        int nz = std::max(10, (int)std::ceil(extZ / bs));
        int ny = std::max(5,  (int)std::ceil(elevRange / bs));

        // Makul sınırlar (büyük veride performans koruması)
        nx = std::min(nx, 80);
        nz = std::min(nz, 80);
        ny = std::min(ny, 40);

        std::cout << "[Scene] Bounding box → Grid: "
                  << nx << "×" << ny << "×" << nz
                  << " @ " << bs << "m\n";
        _bm->reinitialize(nx, ny, nz, bs);
    }

    _dataReady = true;
    _renderer->loadDrillholes(*_db);
    transitionTo(AppStage::DRILLHOLES, state);
    std::cout << "[Scene] Veri yüklendi. 'Kestir' butonuna basın.\n";
}

// ── Tenör kestirim ────────────────────────────────────────────────────────────

void SceneController::runEstimation(AppState& state) {
    if (!_dataReady) {
        std::cout << "[Scene] Önce collar + sample dosyaları yükleyin.\n";
        return;
    }
    if (_lgRunning.load()) {
        std::cout << "[Scene] LG optimizasyonu devam ediyor, lütfen bekleyin.\n";
        return;
    }

    std::cout << "[Scene] Tenör kestirim başlatılıyor ("
              << (state.opt.estimMethod == EstimationMethod::IDW
                  ? "IDW" : "NN")
              << ", şev=" << state.opt.slopeAngleDeg << "°)...\n";

    _bm->estimateFromDatabase(*_db, state.econ, state.opt);
    _renderer->loadBlockModel(*_bm, BlockColorMode::GRADE);
    transitionTo(AppStage::ESTIMATED, state);
}

// ── LG Optimizasyonu ──────────────────────────────────────────────────────────

void SceneController::runOptimization(AppState& state) {
    if (!_dataReady) {
        std::cout << "[Scene] Önce kestirim çalıştırın.\n";
        return;
    }
    if (_lgRunning.load()) {
        std::cout << "[Scene] Optimizasyon zaten çalışıyor.\n";
        return;
    }
    if (_stage == AppStage::EMPTY || _stage == AppStage::DRILLHOLES) {
        // Kestirim yapılmamışsa önce onu çalıştır
        runEstimation(state);
    }
    launchLGThread(state);
    transitionTo(AppStage::OPTIMIZING, state);
}

void SceneController::launchLGThread(AppState& state) {
    if (_lgThread.joinable()) _lgThread.join();
    _lgRunning.store(true);
    _lgDone.store(false);

    // Şev açısı ve blok boyutunu thread'e kopyala (data race'den kaçın)
    const float slopeAngle = state.opt.slopeAngleDeg;
    const int   nx = _bm->nx(), ny = _bm->ny(), nz = _bm->nz();
    const float bs = _bm->blockSize();

    _lgThread = std::thread([this, nx, ny, nz, bs, slopeAngle]() {
        std::cout << "[LG] Thread başladı (α=" << slopeAngle << "°).\n";
        _opt->build(_bm->getBlocks(), nx, ny, nz, bs, slopeAngle);
        _opt->solve();
        _lgRunning.store(false);
        _lgDone.store(true);
        std::cout << "[LG] Thread tamamlandı.\n";
    });
}

void SceneController::finalizeLG(AppState& state) {
    _opt->applyResult(_bm->getBlocks());
    _currentBench = 0;
    _simComplete  = false;
    _lastBenchTime = std::chrono::steady_clock::now();
    rebuildTerrain();
    _renderer->loadBlockModel(*_bm, BlockColorMode::GRADE);
    transitionTo(AppStage::PIT_SHELL, state);
    std::cout << "[Scene] LG bitti — optimal=" << _opt->optimalValue() << "\n"
              << "[Scene] Animasyon için 'Bench Simülasyonu' butonunu kullanın.\n";
}

// ── Tick (her frame) ──────────────────────────────────────────────────────────

bool SceneController::tick(AppState& state) {
    bool changed = false;

    // ── AppEvents işle ────────────────────────────────────────────────────────
    if (state.events.runEstimation) {
        state.events.runEstimation = false;
        runEstimation(state);
        changed = true;
    }
    if (state.events.runOptimization) {
        state.events.runOptimization = false;
        runOptimization(state);
        changed = true;
    }
    if (state.events.exportReport) {
        state.events.exportReport = false;
        if (_stage == AppStage::PIT_SHELL) {
            _bm->generateCSVReport("Pit_Report.csv", state.econ,
                                   state.opt.rockDensity);
        } else {
            std::cout << "[Scene] Rapor sadece PIT_SHELL aşamasında üretilebilir.\n";
        }
    }

    // ── LG thread tamamlandı mı? ──────────────────────────────────────────────
    if (_lgDone.load()) {
        _lgDone.store(false);
        if (_lgThread.joinable()) _lgThread.join();
        finalizeLG(state);
        changed = true;
    }

    // ── Bench animasyonu ──────────────────────────────────────────────────────
    if (_stage == AppStage::PIT_SHELL && state.animating && !_simComplete) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - _lastBenchTime).count();
        if (elapsed >= BENCH_INTERVAL_SEC) {
            _lastBenchTime = now;
            if (advanceBench()) changed = true;
        }
    }

    return changed;
}

// ── Bench ilerleme ────────────────────────────────────────────────────────────

bool SceneController::advanceBench() {
    if (_simComplete) return false;
    const int ny = _bm->ny();
    if (_currentBench >= ny) {
        _simComplete = true;
        std::cout << "[Scene] Simülasyon tamamlandı.\n";
        return false;
    }
    auto& blocks = _bm->getBlocks();
    int mined = 0;
    for (auto& b : blocks) {
        if (b.iy == _currentBench && b.state == LGState::IN_PIT) {
            b.state = LGState::MINED;
            ++mined;
        }
    }
    std::cout << "[Scene] Bench iy=" << _currentBench
              << " — " << mined << " blok kazıldı.\n";
    rebuildTerrain();
    _renderer->loadBlockModel(*_bm, BlockColorMode::GRADE);
    ++_currentBench;
    if (_currentBench >= ny) {
        _simComplete = true;
        std::cout << "[Scene] Simülasyon tamamlandı.\n";
    }
    return true;
}

void SceneController::rebuildTerrain() {
    PitMeshData mesh = _gen->generateFromBlocks(*_bm);
    _renderer->loadMesh(mesh);
    std::cout << "[Scene] Terrain güncellendi ("
              << mesh.vertices.size() << " vertex).\n";
}

// ── Aşama geçişi ─────────────────────────────────────────────────────────────

void SceneController::transitionTo(AppStage s, AppState& state) {
    _stage = state.stage = s;
    // Render bayraklarını aşamaya göre güncelle
    state.render.showDrillholes = false;
    state.render.showBlocks     = false;

    switch (s) {
        case AppStage::EMPTY:
            std::cout << "[Scene] → EMPTY\n";
            break;
        case AppStage::DRILLHOLES:
            std::cout << "[Scene] → DRILLHOLES\n";
            state.render.showDrillholes = true;
            break;
        case AppStage::ESTIMATED:
            std::cout << "[Scene] → ESTIMATED\n";
            state.render.showDrillholes = true;
            state.render.showBlocks     = true;
            break;
        case AppStage::OPTIMIZING:
            std::cout << "[Scene] → OPTIMIZING (LG çalışıyor...)\n";
            state.render.showDrillholes = true;
            state.render.showBlocks     = true;
            break;
        case AppStage::PIT_SHELL:
            std::cout << "[Scene] → PIT_SHELL\n";
            state.render.showDrillholes = true;
            state.render.showBlocks     = true;
            _currentBench  = 0;
            _simComplete   = false;
            _lastBenchTime = std::chrono::steady_clock::now();
            break;
    }
}
