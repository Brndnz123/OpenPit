#include "UI.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

void UI::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

void UI::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UI::newFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UI::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UI::drawMainWindow(AppState& state, SceneController* scene) {
    ImGui::Begin("OpenPit3D Kontrol Paneli");

    ImGui::Text("Veri Durumu: %s", scene->dataReady() ? "Yüklendi" : "Bekleniyor (Dosya Surukle)");
    ImGui::Separator();

    // Görsel Ayarlar
    ImGui::Text("Gorsel Ayarlar");
    ImGui::Checkbox("Sondajlari Goster", &state.render.showDrillholes);
    ImGui::Checkbox("Bloklari Goster", &state.render.showBlocks);
    ImGui::Checkbox("Wireframe", &state.render.showWireframe);
    ImGui::Checkbox("Grid", &state.render.showGrid);
    ImGui::Separator();

    // Optimizasyon Parametreleri
    ImGui::Text("Madencilik & Ekonomik Parametreler");
    ImGui::SliderFloat("Sev Acisi (Derece)", &state.opt.slopeAngleDeg, 20.0f, 85.0f);
    ImGui::InputFloat("Metal Fiyati ($/t)", &state.econ.metalPrice);
    ImGui::InputFloat("Maden Maliyeti ($/t)", &state.econ.miningCost);
    ImGui::InputFloat("Proses Maliyeti ($/t)", &state.econ.processCost);

    float cutOff = state.econ.computeCutoffGrade();
    ImGui::Text("Hesaplanan Cut-Off: %.3f %%", cutOff);
    ImGui::Separator();

    // Aksiyon Butonlari
    if (!scene->dataReady()) ImGui::BeginDisabled();

    if (ImGui::Button("Tenor Kestirimi Baslat (IDW)", ImVec2(-1, 30))) {
        state.events.runEstimation = true;
    }

    if (scene->lgRunning()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "LG Optimizasyonu Calisiyor...");
    }
    else {
        if (ImGui::Button("LG Optimizasyonunu Baslat", ImVec2(-1, 30))) {
            state.events.runOptimization = true;
        }
    }

    if (scene->currentStage() == AppStage::PIT_SHELL) {
        ImGui::Checkbox("Bench Simulasyonu (Animasyon)", &state.animating);
        if (ImGui::Button("CSV Raporu Cikart", ImVec2(-1, 30))) {
            state.events.exportReport = true;
        }
    }

    if (!scene->dataReady()) ImGui::EndDisabled();

    ImGui::End();
}