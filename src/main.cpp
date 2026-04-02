#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <fstream>
#include <vector>
#include <filesystem>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Estructuras de datos
// ---------------------------------------------------------------------------

struct InputFrame {
    int  frame;
    bool pressing;
    bool player2;
};

// ---------------------------------------------------------------------------
// Estado global del mod
// ---------------------------------------------------------------------------

namespace PracticeReplayer {

    static std::vector<InputFrame> g_recording;
    static std::vector<InputFrame> g_savedRecording;
    static bool g_isRecording  = false;
    static bool g_isReplaying  = false;
    static int  g_replayIndex  = 0;
    static int  g_currentFrame = 0;

    bool isModEnabled()    { return Mod::get()->getSettingValue<bool>("mod-enabled");    }
    bool isButtonVisible() { return Mod::get()->getSettingValue<bool>("button-visible"); }
    bool isAutoReplay()    { return Mod::get()->getSettingValue<bool>("auto-replay");    }

    std::filesystem::path savePath() {
        return Mod::get()->getSaveDir() / "practice_recording.dat";
    }

    void saveRecording() {
        if (g_recording.empty()) return;
        g_savedRecording = g_recording;

        std::ofstream file(savePath(), std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return;

        size_t count = g_savedRecording.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& f : g_savedRecording)
            file.write(reinterpret_cast<const char*>(&f), sizeof(InputFrame));
        file.close();

        Notification::create("Grabacion guardada!", NotificationIcon::Success)->show();
    }

    bool loadRecording() {
        auto path = savePath();
        if (!std::filesystem::exists(path)) {
            Notification::create("No hay grabacion guardada.", NotificationIcon::Warning)->show();
            return false;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        size_t count = 0;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (count == 0 || count > 1000000) return false;

        g_savedRecording.resize(count);
        for (auto& f : g_savedRecording)
            file.read(reinterpret_cast<char*>(&f), sizeof(InputFrame));
        file.close();
        return true;
    }

    void startReplay() {
        if (g_savedRecording.empty() && !loadRecording()) return;
        g_isReplaying  = true;
        g_isRecording  = false;
        g_replayIndex  = 0;
        g_currentFrame = 0;
        Notification::create("Reproduciendo grabacion!", NotificationIcon::Info)->show();
    }

    void stopReplay() {
        g_isReplaying = false;
        g_replayIndex = 0;
    }

    void startRecording() {
        g_isReplaying = false;
        g_isRecording = true;
        g_recording.clear();
        g_currentFrame = 0;
        Notification::create("Grabando...", NotificationIcon::Info)->show();
    }

    void stopRecordingAndSave() {
        if (!g_isRecording) return;
        g_isRecording = false;
        saveRecording();
    }
}

// ---------------------------------------------------------------------------
// Hook: GJBaseGameLayer
// ---------------------------------------------------------------------------

class $modify(GJBaseGameLayer) {
    void update(float dt) {
        GJBaseGameLayer::update(dt);

        if (!PracticeReplayer::isModEnabled()) return;
        if (!PracticeReplayer::g_isReplaying)  return;

        auto& saved = PracticeReplayer::g_savedRecording;
        int&  idx   = PracticeReplayer::g_replayIndex;
        int&  frame = PracticeReplayer::g_currentFrame;

        while (idx < (int)saved.size() && saved[idx].frame == frame) {
            auto& inp = saved[idx];
            if (inp.pressing) this->pushButton(1, inp.player2);
            else              this->releaseButton(1, inp.player2);
            idx++;
        }
        frame++;

        if (idx >= (int)saved.size()) {
            PracticeReplayer::stopReplay();
            Notification::create("Reproduccion terminada.", NotificationIcon::Success)->show();
        }
    }

    void pushButton(int button, bool isPlayer2) {
        GJBaseGameLayer::pushButton(button, isPlayer2);
        if (!PracticeReplayer::isModEnabled()) return;
        if (!PracticeReplayer::g_isRecording)  return;
        PracticeReplayer::g_recording.push_back({ PracticeReplayer::g_currentFrame, true, isPlayer2 });
    }

    void releaseButton(int button, bool isPlayer2) {
        GJBaseGameLayer::releaseButton(button, isPlayer2);
        if (!PracticeReplayer::isModEnabled()) return;
        if (!PracticeReplayer::g_isRecording)  return;
        PracticeReplayer::g_recording.push_back({ PracticeReplayer::g_currentFrame, false, isPlayer2 });
    }
};

// ---------------------------------------------------------------------------
// Hook: PlayLayer
// ---------------------------------------------------------------------------

class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        PracticeReplayer::g_isRecording  = false;
        PracticeReplayer::g_isReplaying  = false;
        PracticeReplayer::g_currentFrame = 0;
        PracticeReplayer::g_replayIndex  = 0;
        PracticeReplayer::g_recording.clear();

        if (PracticeReplayer::isModEnabled() && PracticeReplayer::isAutoReplay()) {
            PracticeReplayer::loadRecording();
            if (!PracticeReplayer::g_savedRecording.empty())
                PracticeReplayer::startReplay();
        }
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        PracticeReplayer::g_currentFrame = 0;
        PracticeReplayer::g_replayIndex  = 0;
        PracticeReplayer::g_recording.clear();
    }
};

// ---------------------------------------------------------------------------
// Hook: PauseLayer — con nombre de clase explicito para los selectores
// ---------------------------------------------------------------------------

class $modify(PRPauseLayer, PauseLayer) {

    // Callbacks del menu
    void onRecord(CCObject*) {
        this->onResume(nullptr);
        PracticeReplayer::startRecording();
    }

    void onReplay(CCObject*) {
        this->onResume(nullptr);
        PracticeReplayer::startReplay();
    }

    void onSave(CCObject*) {
        PracticeReplayer::stopRecordingAndSave();
    }

    void onToggleMod(CCObject* sender) {
        bool nowEnabled = !PracticeReplayer::isModEnabled();
        Mod::get()->setSettingValue("mod-enabled", nowEnabled);

        if (!nowEnabled) {
            PracticeReplayer::g_isRecording = false;
            PracticeReplayer::g_isReplaying = false;
        }

        if (auto btn = typeinfo_cast<CCMenuItemLabel*>(sender)) {
            if (auto lbl = typeinfo_cast<CCLabelBMFont*>(btn->getLabel()))
                lbl->setString(nowEnabled ? "Mod: ON" : "Mod: OFF");
        }

        Notification::create(
            nowEnabled ? "Mod activado!" : "Mod desactivado!",
            nowEnabled ? NotificationIcon::Success : NotificationIcon::Warning
        )->show();
    }

    void onToggleVisible(CCObject*) {
        bool nowVisible = !PracticeReplayer::isButtonVisible();
        Mod::get()->setSettingValue("button-visible", nowVisible);
        Notification::create(
            nowVisible ? "Boton visible" : "Boton oculto",
            NotificationIcon::Info
        )->show();
    }

    // Setup del panel en el menu de pausa
    void customSetup() {
        PauseLayer::customSetup();

        if (!PracticeReplayer::isButtonVisible()) return;

        auto winSize = CCDirector::get()->getWinSize();

        // Fondo del panel
        auto bg = CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({ 155.f, 140.f });
        bg->setOpacity(210);
        bg->setPosition({ winSize.width - 88.f, winSize.height - 82.f });
        bg->setZOrder(10);
        this->addChild(bg);

        // Titulo
        auto title = CCLabelBMFont::create("Practice\nReplayer", "goldFont.fnt");
        title->setScale(0.45f);
        title->setAlignment(kCCTextAlignmentCenter);
        title->setPosition({ winSize.width - 88.f, winSize.height - 48.f });
        title->setZOrder(11);
        this->addChild(title);

        // Menu de botones
        auto menu = CCMenu::create();
        menu->setPosition({ winSize.width - 88.f, winSize.height - 82.f });
        menu->setZOrder(12);

        // Grabar
        auto recLbl = CCLabelBMFont::create("  Grabar  ", "bigFont.fnt");
        recLbl->setScale(0.5f);
        recLbl->setColor({ 255, 100, 100 });
        auto recBtn = CCMenuItemLabel::create(recLbl, this, menu_selector(PRPauseLayer::onRecord));
        recBtn->setPosition({ 0.f, 22.f });
        menu->addChild(recBtn);

        // Reproducir
        auto repLbl = CCLabelBMFont::create("Reproducir", "bigFont.fnt");
        repLbl->setScale(0.5f);
        repLbl->setColor({ 100, 200, 255 });
        auto repBtn = CCMenuItemLabel::create(repLbl, this, menu_selector(PRPauseLayer::onReplay));
        repBtn->setPosition({ 0.f, 2.f });
        menu->addChild(repBtn);

        // Guardar
        auto savLbl = CCLabelBMFont::create("  Guardar  ", "bigFont.fnt");
        savLbl->setScale(0.5f);
        savLbl->setColor({ 100, 255, 130 });
        auto savBtn = CCMenuItemLabel::create(savLbl, this, menu_selector(PRPauseLayer::onSave));
        savBtn->setPosition({ 0.f, -18.f });
        menu->addChild(savBtn);

        // Activar/Desactivar mod
        auto togLbl = CCLabelBMFont::create(
            PracticeReplayer::isModEnabled() ? "Mod: ON" : "Mod: OFF",
            "bigFont.fnt"
        );
        togLbl->setScale(0.5f);
        auto togBtn = CCMenuItemLabel::create(togLbl, this, menu_selector(PRPauseLayer::onToggleMod));
        togBtn->setPosition({ 0.f, -38.f });
        menu->addChild(togBtn);

        // Ocultar boton
        auto hidLbl = CCLabelBMFont::create("Ocultar btn", "bigFont.fnt");
        hidLbl->setScale(0.4f);
        hidLbl->setColor({ 180, 180, 180 });
        auto hidBtn = CCMenuItemLabel::create(hidLbl, this, menu_selector(PRPauseLayer::onToggleVisible));
        hidBtn->setPosition({ 0.f, -56.f });
        menu->addChild(hidBtn);

        this->addChild(menu);
    }
};
