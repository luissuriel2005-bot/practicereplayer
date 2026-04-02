#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <fstream>
#include <vector>
#include <filesystem>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Estructuras
// ---------------------------------------------------------------------------

struct InputFrame {
    int  frame;
    bool pressing;
    bool player2;
};

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

namespace PR {
    static std::vector<InputFrame> g_recording;
    static std::vector<InputFrame> g_saved;
    static bool g_recording_active = false;
    static bool g_replaying        = false;
    static int  g_replay_idx       = 0;
    static int  g_frame            = 0;

    bool modEnabled()    { return Mod::get()->getSettingValue<bool>("mod-enabled");    }
    bool btnVisible()    { return Mod::get()->getSettingValue<bool>("button-visible"); }
    bool autoReplay()    { return Mod::get()->getSettingValue<bool>("auto-replay");    }

    std::filesystem::path savePath() {
        return Mod::get()->getSaveDir() / "recording.dat";
    }

    void saveRecording() {
        if (g_recording.empty()) return;
        g_saved = g_recording;
        std::ofstream f(savePath(), std::ios::binary | std::ios::trunc);
        if (!f) return;
        size_t n = g_saved.size();
        f.write(reinterpret_cast<const char*>(&n), sizeof(n));
        for (auto& fr : g_saved)
            f.write(reinterpret_cast<const char*>(&fr), sizeof(fr));
        Notification::create("Grabacion guardada!", NotificationIcon::Success)->show();
    }

    bool loadRecording() {
        auto p = savePath();
        if (!std::filesystem::exists(p)) {
            Notification::create("Sin grabacion guardada.", NotificationIcon::Warning)->show();
            return false;
        }
        std::ifstream f(p, std::ios::binary);
        if (!f) return false;
        size_t n = 0;
        f.read(reinterpret_cast<char*>(&n), sizeof(n));
        if (n == 0 || n > 2000000) return false;
        g_saved.resize(n);
        for (auto& fr : g_saved)
            f.read(reinterpret_cast<char*>(&fr), sizeof(fr));
        return true;
    }

    void startRecord() {
        g_replaying        = false;
        g_recording_active = true;
        g_recording.clear();
        g_frame = 0;
        Notification::create("Grabando inputs...", NotificationIcon::Info)->show();
    }

    void stopAndSave() {
        if (!g_recording_active) return;
        g_recording_active = false;
        saveRecording();
    }

    void startReplay() {
        if (g_saved.empty() && !loadRecording()) return;
        g_replaying        = true;
        g_recording_active = false;
        g_replay_idx       = 0;
        g_frame            = 0;
        Notification::create("Reproduciendo!", NotificationIcon::Info)->show();
    }

    void stopReplay() {
        g_replaying  = false;
        g_replay_idx = 0;
    }

    void resetState() {
        g_recording_active = false;
        g_replaying        = false;
        g_frame            = 0;
        g_replay_idx       = 0;
        g_recording.clear();
    }
}

// ---------------------------------------------------------------------------
// Callbacks separados (clase propia para menu_selector)
// ---------------------------------------------------------------------------

class PRCallbacks : public CCObject {
public:
    static PRCallbacks* get() {
        static PRCallbacks* inst = nullptr;
        if (!inst) { inst = new PRCallbacks(); inst->retain(); }
        return inst;
    }

    void onRecord(CCObject*) {
        // Reanudar el nivel
        if (auto pl = PlayLayer::get()) {
            if (auto pause = pl->m_uiLayer) {
                // Buscar PauseLayer activo y llamar resume
            }
        }
        PR::startRecord();
        // Cerrar pausa
        if (auto scene = CCDirector::get()->getRunningScene()) {
            scene->removeChildByTag(9821);
        }
        if (auto pl = PlayLayer::get()) pl->resumeGame();
    }

    void onReplay(CCObject*) {
        PR::startReplay();
        if (auto pl = PlayLayer::get()) pl->resumeGame();
    }

    void onSave(CCObject*) {
        PR::stopAndSave();
    }

    void onToggleMod(CCObject* sender) {
        bool now = !PR::modEnabled();
        Mod::get()->setSettingValue("mod-enabled", now);
        if (!now) { PR::g_recording_active = false; PR::g_replaying = false; }

        // Actualizar texto del boton
        if (auto item = typeinfo_cast<CCMenuItemLabel*>(sender))
            if (auto lbl = typeinfo_cast<CCLabelBMFont*>(item->getLabel()))
                lbl->setString(now ? "Mod: ON" : "Mod: OFF");

        Notification::create(
            now ? "Mod activado!" : "Mod desactivado!",
            now ? NotificationIcon::Success : NotificationIcon::Warning
        )->show();
    }

    void onHideBtn(CCObject*) {
        Mod::get()->setSettingValue("button-visible", false);
        Notification::create("Boton oculto. Reactiva en ajustes del mod.", NotificationIcon::Info)->show();
    }
};

// ---------------------------------------------------------------------------
// Hook: GJBaseGameLayer - inputs y reproduccion
// ---------------------------------------------------------------------------

class $modify(GJBaseGameLayer) {
    void update(float dt) {
        GJBaseGameLayer::update(dt);
        if (!PR::modEnabled() || !PR::g_replaying) return;

        auto& saved = PR::g_saved;
        int&  idx   = PR::g_replay_idx;
        int&  frame = PR::g_frame;

        while (idx < (int)saved.size() && saved[idx].frame == frame) {
            auto& inp = saved[idx];
            if (inp.pressing) this->pushButton(1, inp.player2);
            else              this->releaseButton(1, inp.player2);
            idx++;
        }
        frame++;

        if (idx >= (int)saved.size()) {
            PR::stopReplay();
            Notification::create("Reproduccion terminada.", NotificationIcon::Success)->show();
        }
    }

    void pushButton(int button, bool isPlayer2) {
        GJBaseGameLayer::pushButton(button, isPlayer2);
        if (PR::modEnabled() && PR::g_recording_active)
            PR::g_recording.push_back({ PR::g_frame, true, isPlayer2 });
    }

    void releaseButton(int button, bool isPlayer2) {
        GJBaseGameLayer::releaseButton(button, isPlayer2);
        if (PR::modEnabled() && PR::g_recording_active)
            PR::g_recording.push_back({ PR::g_frame, false, isPlayer2 });
    }
};

// ---------------------------------------------------------------------------
// Hook: PlayLayer
// ---------------------------------------------------------------------------

class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        PR::resetState();
        if (PR::modEnabled() && PR::autoReplay()) {
            PR::loadRecording();
            if (!PR::g_saved.empty()) PR::startReplay();
        }
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        PR::g_frame      = 0;
        PR::g_replay_idx = 0;
        PR::g_recording.clear();
    }
};

// ---------------------------------------------------------------------------
// Hook: PauseLayer - agregar panel usando create
// ---------------------------------------------------------------------------

class $modify(PRPauseLayer, PauseLayer) {
    static PauseLayer* create(bool p0) {
        auto* ret = PauseLayer::create(p0);
        if (!ret || !PR::btnVisible()) return ret;

        auto* cb      = PRCallbacks::get();
        auto  winSize = CCDirector::get()->getWinSize();

        // Posicion del panel: esquina superior derecha
        float px = winSize.width  - 85.f;
        float py = winSize.height - 80.f;

        // Fondo
        auto* bg = CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({ 150.f, 155.f });
        bg->setOpacity(215);
        bg->setPosition({ px, py });
        bg->setZOrder(10);
        ret->addChild(bg);

        // Titulo
        auto* title = CCLabelBMFont::create("Practice\nReplayer", "goldFont.fnt");
        title->setScale(0.44f);
        title->setAlignment(kCCTextAlignmentCenter);
        title->setPosition({ px, py + 55.f });
        title->setZOrder(11);
        ret->addChild(title);

        // Menu
        auto* menu = CCMenu::create();
        menu->setPosition({ px, py });
        menu->setZOrder(12);

        // Grabar
        auto* lRec = CCLabelBMFont::create("  Grabar  ", "bigFont.fnt");
        lRec->setScale(0.48f); lRec->setColor({ 255, 90, 90 });
        auto* bRec = CCMenuItemLabel::create(lRec, cb, menu_selector(PRCallbacks::onRecord));
        bRec->setPosition({ 0.f, 25.f });
        menu->addChild(bRec);

        // Reproducir
        auto* lRep = CCLabelBMFont::create("Reproducir", "bigFont.fnt");
        lRep->setScale(0.48f); lRep->setColor({ 90, 200, 255 });
        auto* bRep = CCMenuItemLabel::create(lRep, cb, menu_selector(PRCallbacks::onReplay));
        bRep->setPosition({ 0.f, 5.f });
        menu->addChild(bRep);

        // Guardar
        auto* lSav = CCLabelBMFont::create("  Guardar  ", "bigFont.fnt");
        lSav->setScale(0.48f); lSav->setColor({ 90, 235, 120 });
        auto* bSav = CCMenuItemLabel::create(lSav, cb, menu_selector(PRCallbacks::onSave));
        bSav->setPosition({ 0.f, -15.f });
        menu->addChild(bSav);

        // Toggle mod
        auto* lTog = CCLabelBMFont::create(
            PR::modEnabled() ? "Mod: ON" : "Mod: OFF", "bigFont.fnt");
        lTog->setScale(0.48f);
        auto* bTog = CCMenuItemLabel::create(lTog, cb, menu_selector(PRCallbacks::onToggleMod));
        bTog->setPosition({ 0.f, -35.f });
        menu->addChild(bTog);

        // Ocultar boton
        auto* lHide = CCLabelBMFont::create("Ocultar btn", "bigFont.fnt");
        lHide->setScale(0.38f); lHide->setColor({ 180, 180, 180 });
        auto* bHide = CCMenuItemLabel::create(lHide, cb, menu_selector(PRCallbacks::onHideBtn));
        bHide->setPosition({ 0.f, -53.f });
        menu->addChild(bHide);

        ret->addChild(menu);
        return ret;
    }
};
