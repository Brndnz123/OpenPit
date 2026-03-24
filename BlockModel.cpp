#include "BlockModel.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <limits>

#ifdef _OPENMP
  #include <omp.h>
#endif

// ── SpatialIndex ──────────────────────────────────────────────────────────────

void SpatialIndex::insert(int idx, glm::vec3 pos) {
    auto [cx, cy, cz] = toCell(pos);
    _cells[key(cx, cy, cz)].push_back(idx);
}

std::vector<int> SpatialIndex::query(glm::vec3 pos, int r) const {
    auto [cx, cy, cz] = toCell(pos);
    std::vector<int> result;
    for (int dx = -r; dx <= r; ++dx)
        for (int dy = -r; dy <= r; ++dy)
            for (int dz = -r; dz <= r; ++dz) {
                auto it = _cells.find(key(cx + dx, cy + dy, cz + dz));
                if (it != _cells.end())
                    for (int i : it->second) result.push_back(i);
            }
    return result;
}

// ── Kurucu ve grid inşası ─────────────────────────────────────────────────────

BlockModel::BlockModel(int nx, int ny, int nz, float bs)
    : _nx(nx), _ny(ny), _nz(nz), _blockSize(bs)
{
    buildGrid();
}

void BlockModel::reinitialize(int nx, int ny, int nz, float bs) {
    _nx = nx; _ny = ny; _nz = nz; _blockSize = bs;
    _blocks.clear();
    buildGrid();
    std::cout << "[BlockModel] Reinitialized: "
              << nx << "x" << ny << "x" << nz
              << " @ " << bs << "m  (" << _blocks.size() << " blocks)\n";
}

void BlockModel::buildGrid() {
    _blocks.reserve(_nx * _ny * _nz);
    for (int iy = 0; iy < _ny; ++iy)
        for (int iz = 0; iz < _nz; ++iz)
            for (int ix = 0; ix < _nx; ++ix) {
                Block b;
                b.id = (int)_blocks.size();
                b.ix = ix; b.iy = iy; b.iz = iz;
                // worldPos: iy=0 en üst bench (y ≈ 0), derinleştikçe negatif
                b.worldPos = glm::vec3(
                    (ix - _nx / 2) * _blockSize,
                    -(iy + 0.5f)   * _blockSize,
                    (iz - _nz / 2) * _blockSize);
                b.grade = 0.f; b.value = 0.f;
                b.state = LGState::ACTIVE;
                _blocks.push_back(b);
            }
    std::cout << "[BlockModel] Grid built: "
              << _nx << "x" << _ny << "x" << _nz
              << " @ " << _blockSize << "m  ("
              << _blocks.size() << " blocks)\n";
}

void BlockModel::resetStates() {
    for (auto& b : _blocks) b.state = LGState::ACTIVE;
}

// ── Blok ekonomik değeri ──────────────────────────────────────────────────────

static float blockValue(float grade, float gc, const EconomicParams& e) {
    if (grade >= gc)
        return grade * e.recovery * (e.metalPrice - e.sellingCost)
               - e.miningCost - e.processCost;
    return -e.miningCost;
}

// ── IDW (paralel) ─────────────────────────────────────────────────────────────

void BlockModel::runIDW(const std::vector<WorldPoint>& samples,
                        const EconomicParams& econ,
                        float searchRadius, int power) {
    const float gc = econ.computeCutoffGrade();
    if (gc >= FLT_MAX * 0.5f)
        std::cout << "[BlockModel] UYARI: ekonomik olmayan parametre — tüm bloklar waste.\n";
    else
        std::cout << "[BlockModel] Lane kesim tenörü: " << gc << " %\n";

    const float cellSize = _blockSize * 2.f;
    const float sr       = (searchRadius > 0.f) ? searchRadius : _blockSize * SEARCH_MULT;
    const float r2cap    = sr * sr;
    const double pw      = (power > 0) ? (double)power : (double)IDW_POWER;

    SpatialIndex idx(cellSize);
    for (int i = 0; i < (int)samples.size(); ++i) idx.insert(i, samples[i].pos);

    const int N = (int)_blocks.size();
    int estimated = 0;

    #pragma omp parallel for schedule(dynamic, 64) reduction(+:estimated)
    for (int i = 0; i < N; ++i) {
        Block& b = _blocks[i];
        auto candidates = idx.query(b.worldPos, 3);
        double sumW = 0, sumWG = 0;
        bool hit = false;
        for (int ci : candidates) {
            glm::vec3 d = b.worldPos - samples[ci].pos;
            float d2 = glm::dot(d, d);
            if (d2 > r2cap) continue;
            if (d2 < 1e-4f) {
                b.grade = samples[ci].grade;
                b.value = blockValue(b.grade, gc, econ);
                hit = true; ++estimated; break;
            }
            double w = 1.0 / std::pow((double)d2, pw * 0.5);
            sumWG += samples[ci].grade * w;
            sumW  += w;
        }
        if (!hit && sumW > 0) {
            b.grade = (float)(sumWG / sumW);
            b.value = blockValue(b.grade, gc, econ);
            ++estimated;
        }
        if (!hit && sumW == 0)
            b.value = -econ.miningCost;
    }

    std::cout << "[BlockModel] IDW tamamlandı: " << estimated << "/" << N;
#ifdef _OPENMP
    std::cout << "  [" << omp_get_max_threads() << " thread]";
#endif
    std::cout << "\n";
}

// ── Nearest Neighbor ──────────────────────────────────────────────────────────

void BlockModel::runNearestNeighbor(const std::vector<WorldPoint>& samples,
                                    const EconomicParams& econ) {
    const float gc = econ.computeCutoffGrade();
    std::cout << "[BlockModel] NN başlatıldı (" << samples.size() << " örnek)...\n";

    const int N = (int)_blocks.size();

    #pragma omp parallel for schedule(dynamic, 64)
    for (int i = 0; i < N; ++i) {
        Block& b = _blocks[i];
        float bestD2 = std::numeric_limits<float>::max();
        float bestG  = 0.f;
        for (const auto& s : samples) {
            glm::vec3 d = b.worldPos - s.pos;
            float d2 = glm::dot(d, d);
            if (d2 < bestD2) { bestD2 = d2; bestG = s.grade; }
        }
        b.grade = bestG;
        b.value = blockValue(b.grade, gc, econ);
    }

    std::cout << "[BlockModel] NN tamamlandı: " << N << " blok.\n";
}

// ── Ana kestirim giriş noktası ────────────────────────────────────────────────

void BlockModel::estimateFromDatabase(const DrillholeDatabase& db,
                                       const EconomicParams& econ,
                                       const PitOptimizationParams& opt) {
    resetStates();

    auto samples = db.getDesurveyedSamples();
    if (samples.empty()) {
        std::cerr << "[BlockModel] Örnek yok — collar + sample dosyaları yükleyin.\n";
        return;
    }

    switch (opt.estimMethod) {
        case EstimationMethod::IDW:
            runIDW(samples, econ, opt.searchRadius, opt.idwPower);
            break;
        case EstimationMethod::NEAREST_NEIGHBOR:
            runNearestNeighbor(samples, econ);
            break;
    }
}

// ── CSV Raporu ────────────────────────────────────────────────────────────────
// (Orijinal implementasyon korundu, lithology referansları kaldırıldı)

bool BlockModel::generateCSVReport(const std::string& filepath,
                                    const EconomicParams& econ,
                                    float rockDensity) const {
    std::cout << "[Report] Ekonomik parametreler:\n"
              << "         metalPrice="  << econ.metalPrice
              << "  recovery="           << econ.recovery
              << "  miningCost="         << econ.miningCost
              << "  processCost="        << econ.processCost
              << "  sellingCost="        << econ.sellingCost << "\n";

    const float metalPrice  = (econ.metalPrice  > 0.f)                         ? econ.metalPrice  : 100.f;
    const float recovery    = (econ.recovery > 0.f && econ.recovery <= 1.f)    ? econ.recovery    : 0.90f;
    const float miningCost  = (econ.miningCost  > 0.f)                         ? econ.miningCost  : 30.f;
    const float processCost = (econ.processCost >= 0.f)                         ? econ.processCost : 20.f;
    const float sellingCost = (econ.sellingCost >= 0.f)                         ? econ.sellingCost : 5.f;

    float gc = 0.f;
    {
        float netSmelter = metalPrice * recovery - miningCost;
        if (netSmelter <= 0.f) { gc = 0.f; }
        else {
            float g_m = (processCost + sellingCost) / netSmelter;
            float g_p =  processCost                / netSmelter;
            float g_k = (processCost + sellingCost) / (metalPrice * recovery);
            gc = std::max({ std::min(g_m, g_p), std::min(g_p, g_k), std::min(g_m, g_k) });
            gc = std::max(0.f, gc);
        }
    }

    {
        int nActive=0, nInPit=0, nDiscarded=0, nMined=0;
        for (const auto& b : _blocks) {
            switch (b.state) {
                case LGState::ACTIVE:    ++nActive;    break;
                case LGState::IN_PIT:    ++nInPit;     break;
                case LGState::DISCARDED: ++nDiscarded; break;
                case LGState::MINED:     ++nMined;     break;
            }
        }
        if (nInPit == 0 && nMined == 0) {
            std::cerr << "[Report] HATA: IN_PIT veya MINED blok bulunamadı. "
                         "LG optimizasyonu çalıştırılmadı mı?\n";
            return false;
        }
    }

    const float blockVol    = _blockSize * _blockSize * _blockSize;
    const float blockTonnes = blockVol * rockDensity;
    double oreTonnes=0, wasteTonnes=0, sumGradeOre=0, netNPV=0;
    long   oreBlocks=0, wasteBlocks=0;

    for (const auto& b : _blocks) {
        if (b.state != LGState::IN_PIT && b.state != LGState::MINED) continue;
        netNPV += (double)b.value;
        if (b.grade >= gc) {
            oreTonnes += blockTonnes; sumGradeOre += b.grade; ++oreBlocks;
        } else {
            wasteTonnes += blockTonnes; ++wasteBlocks;
        }
    }

    const double avgGrade   = (oreBlocks > 0) ? sumGradeOre / oreBlocks : 0.0;
    const double stripRatio = (oreTonnes  > 0) ? wasteTonnes / oreTonnes : 0.0;

    std::ofstream f(filepath);
    if (!f.is_open()) { std::cerr << "[Report] Dosya açılamadı: " << filepath << "\n"; return false; }

    f << std::fixed;
    f << "LABEL,VALUE,UNIT\n";
    f << "--- PIT OZETI ---,,\n";
    f << "Ore Blok," << oreBlocks << ",adet\n";
    f << "Waste Blok," << wasteBlocks << ",adet\n";
    f << "Ore Tonaj," << std::setprecision(1) << oreTonnes    << ",t\n";
    f << "Waste Tonaj," << std::setprecision(1) << wasteTonnes << ",t\n";
    f << "Stripping Orani," << std::setprecision(3) << stripRatio << ",t waste/t ore\n";
    f << "Ort. Ore Tenoru," << std::setprecision(4) << avgGrade  << ",%\n";
    f << "Net Pit Degeri," << std::setprecision(2) << netNPV   << ",$\n";
    f << "--- EKONOMIK PARAMETRELER ---,,\n";
    f << "Metal Fiyati," << std::setprecision(2) << metalPrice  << ",$/t metal\n";
    f << "Recovery," << std::setprecision(1) << recovery*100.f << ",%\n";
    f << "Maden Maliyeti," << std::setprecision(2) << miningCost  << ",$/t kaya\n";
    f << "Proses Maliyeti," << std::setprecision(2) << processCost << ",$/t ore\n";
    f << "Satis Maliyeti," << std::setprecision(2) << sellingCost << ",$/t metal\n";
    f << "Kesim Tenoru (gc)," << std::setprecision(4) << gc        << ",%\n";
    f << "Kaya Yogunlugu," << std::setprecision(2) << rockDensity  << ",t/m3\n";
    f << "--- MODEL BILGISI ---,,\n";
    f << "Grid," << _nx << "x" << _ny << "x" << _nz << ",blok\n";
    f << "Blok Boyutu," << std::setprecision(1) << _blockSize << ",m\n";
    f.close();

    std::error_code ec;
    auto abs = std::filesystem::absolute(filepath, ec);
    std::cout << "[Report] Kaydedildi: " << (ec ? filepath : abs.string()) << "\n";
    return true;
}
