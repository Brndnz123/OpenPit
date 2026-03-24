#pragma once
#include <vector>
#include "Types.h"

// ── Lerchs-Grossmann Maksimum Akış Optimizasyonu ────────────────────────────
//
//  Blok bağımlılığı (koni koşulu), kullanıcının girdiği şev açısına göre
//  dinamik olarak hesaplanır.  Formül (build() içinde uygulanır):
//
//    Δy_world = (iy_üst – iy_alt) * blockSize          [bench farkı, metre]
//    Δd_world = sqrt((Δix² + Δiz²)) * blockSize          [yatay mesafe, metre]
//    R        = tan(slopeAngleDeg × π/180) × Δy_world   [arama yarıçapı, metre]
//    Bağımlılık: üst blok, alt bloğun kazılabilmesi için ön koşuldur
//                ancak ve ancak Δd_world ≤ R ise.
//
//  Bu yaklaşım sabit slopeCells tamsayısının yetersiz kaldığı kırıklı
//  yüzeylerde ve non-integer açılarda çok daha doğru sonuç verir.

class LGOptimizer {
public:
    // Tüm build/solve/apply adımlarını tek seferde uygular.
    void optimize(std::vector<Block>& blocks,
                  int nx, int ny, int nz,
                  float blockSize,
                  float slopeAngleDeg = 45.f);

    // Adım adım kullanım (SceneController'ın thread'ini yönetmesi için):
    void build   (std::vector<Block>& blocks,
                  int nx, int ny, int nz,
                  float blockSize,
                  float slopeAngleDeg = 45.f);
    double solve();
    void applyResult(std::vector<Block>& blocks);

    // Sonuç sorguları
    double optimalValue() const { return _positiveSum - _maxFlow; }
    double positiveSum()  const { return _positiveSum; }
    double maxFlow()      const { return _maxFlow; }

private:
    struct Edge { int to, rev; float cap; };

    int _numNodes = 0, _S = 0, _T = 0;
    std::vector<std::vector<Edge>> _graph;
    std::vector<int>  _level, _iter;
    double _positiveSum = 0.0, _maxFlow = 0.0;

    void  addEdge(int u, int v, float cap);
    bool  bfs();
    float dfs(int v, float pushed);
};
