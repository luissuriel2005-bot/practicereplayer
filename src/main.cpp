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

    // Lee la config de Geode
    bool isModEnabled()    { return Mod::get()->getSettingValue<bool>("mod-enabled");   }
    bool isButtonVisible() { return Mod::get()->getSettingValue<bool>("button-visible"); }
    bool isAutoReplay()    { return Mod::get()->getSettingValue<bool>("auto-replay");    }

    // Ruta donde se guarda la grabacion
    std::filesystem::path savePath() {
        return Mod::get()->getSaveDir() / "practice_recording.dat";
    }

    // Guarda la grabacion en disco
    void saveRecording() {
        if (g_recording.empty()) return;
        g_savedRecording = g_recording;

        std::ofstream file(savePath(), std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return;

        size_t count = g_savedRecording.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& f : g_savedRecording) {
            file.write(reinterpret_cast<const char*>(&f), sizeof(InputFrame));
        }
        file.close();

        Notification::create("Grabacion guardada!", NotificationIcon::Success)->show();
    }

    // Carga la grabacion desde disco
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
        for (auto& f : g_savedRecording) {
            file.read(reinterpret_cast<char*>(&f), sizeof(InputFrame));
        }
        file.close();
        return true;
    }

    // Inicia la reproduccion
    void startReplay() {
        if (g_savedRecording.empty() && !loadRecording()) return;
        g_isReplaying  = true;
        g_isRecording  = false;
        g_replayIndex  = 0;
        g_currentFrame = 0;
        Notification::create("Reproduciendo grabacion!", NotificationIcon::Info)->show();
    }

    // Para la reproduccion
    void stopReplay() {
        g_isReplaying = false;
        g_replayIndex = 0;
    }

    // Inicia la grabacion
    void startRecording() {
        g_isReplaying = false;
        g_isRecording = true;
        g_recording.clear();
        g_currentFrame = 0;
        Notification::create("Grabando...", NotificationIcon::Info)->show();
    }

    // Para la grabacion y guarda
    void stopRecordingAndSave() {
        if (!g_isRecording) return;
        g_isRecording = false;
        saveRecording();
    }
}

// ---------------------------------------------------------------------------
// Hook: GJBaseGameLayer - maneja los inputs y el avance de frames
// ---------------------------------------------------------------------------

class $modify(GJBaseGameLayer) {

    // Se llama cada frame mientras se juega
    void update(float dt) {
        GJBaseGameLayer::update(dt);

        if (!PracticeReplayer::isModEnabled()) return;
        if (!PracticeReplayer::g_isReplaying)  return;

        auto& saved  = PracticeReplayer::g_savedRecording;
        int&  idx    = PracticeReplayer::g_replayIndex;
        int&  frame  = PracticeReplayer::g_currentFrame;

        // Reproducir todos los frames que correspondan al frame actual
        while (idx < (int)saved.size() && saved[idx].frame == frame) {
            auto& inp = saved[idx];
            if (inp.player2) {
                if (inp.pressing) this->pushButton(1, true);
                else              this->releaseButton(1, true);
            } else {
                if (inp.pressing) this->pushButton(1, false);
                else              this->releaseButton(1, false);
            }
            idx++;
        }

        frame++;

        // Fin de la reproduccion
        if (idx >= (int)saved.size()) {
            PracticeReplayer::stopReplay();
            Notification::create("Reproduccion terminada.", NotificationIcon::Success)->show();
        }
    }

    // Captura pulsaciones durante la grabacion
    void pushButton(int button, bool isPlayer2) {
        GJBaseGameLayer::pushButton(button, isPlayer2);

        if (!PracticeReplayer::isModEnabled()) return;
        if (!PracticeReplayer::g_isRecording)  return;

        PracticeReplayer::g_recording.push_back({
            PracticeReplayer::g_currentFrame,
            true,
            isPlayer2
        });
    }

    void releaseButton(int button, bool isPlayer2) {
        GJBaseGameLayer::releaseButton(button, isPlayer2);

        if (!PracticeReplayer::isModEnabled()) return;
        if (!PracticeReplayer::g_isRecording)  return;

        PracticeReplayer::g_recording.push_back({
            PracticeReplayer::g_currentFrame,
            false,
            isPlayer2
        });
    }
};

// ---------------------------------------------------------------------------
// Hook: PlayLayer - resetea estado al iniciar/reiniciar nivel
// ---------------------------------------------------------------------------

class $modify(PlayLayer) {

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        // Limpia el estado al cargar el nivel
        PracticeReplayer::g_isRecording  = false;
        PracticeReplayer::g_isReplaying  = false;
        PracticeReplayer::g_currentFrame = 0;
        PracticeReplayer::g_replayIndex  = 0;
        PracticeReplayer::g_recording.clear();

        // Reproduccion automatica si esta activada
        if (PracticeReplayer::isModEnabled() && PracticeReplayer::isAutoReplay()) {
            PracticeReplayer::loadRecording();
            if (!PracticeReplayer::g_savedRecording.empty()) {
                PracticeReplayer::startReplay();
            }
        }

        return true;
    }

    // Al reiniciar el nivel desde el inicio en practica
    void resetLevel() {
        PlayLayer::resetLevel();
        PracticeReplayer::g_currentFrame = 0;
        PracticeReplayer::g_replayIndex  = 0;
        PracticeReplayer::g_recording.clear();
    }
};

// ---------------------------------------------------------------------------
// Boton personalizado en la UI del nivel (esquina superior derecha)
// ---------------------------------------------------------------------------

class PracticeReplayerButton : public CCMenu {
public:
    CCMenuItemToggler* m_toggle = nullptr;

    static PracticeReplayerButton* create() {
        auto ret = new PracticeReplayerButton();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init() {
        if (!CCMenu::init()) return false;

        // Sprite ON / OFF usando sprites de Geode
        auto onSpr  = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        auto offSpr = CCSprite::createWithSpriteFrameName("GJ_stopBtn2_001.png");

        if (!onSpr || !offSpr) {
            // Fallback con texto si no hay sprites
            auto onLbl  = CCLabelBMFont::create("REC", "bigFont.fnt");
            auto offLbl = CCLabelBMFont::create("STOP", "bigFont.fnt");
            onSpr  = CCSprite::create();
            offSpr = CCSprite::create();
        }

        onSpr->setScale(0.5f);
        offSpr->setScale(0.5f);

        m_toggle = CCMenuItemToggler::create(
            offSpr, onSpr, this, menu_selector(PracticeReplayerButton::onToggle)
        );
        m_toggle->setTag(1);

        this->addChild(m_toggle);
        this->setPosition({0, 0});
        return true;
    }

    void onToggle(CCObject*) {
        bool nowEnabled = !PracticeReplayer::isModEnabled();
        Mod::get()->setSettingValue("mod-enabled", nowEnabled);

        if (nowEnabled) {
            Notification::create("Practice Replayer: ON", NotificationIcon::Success)->show();
        } else {
            PracticeReplayer::g_isRecording = false;
            PracticeReplayer::g_isReplaying = false;
            Notification::create("Practice Replayer: OFF", NotificationIcon::Warning)->show();
        }
    }
};

// ---------------------------------------------------------------------------
// Hook: PauseLayer - agrega el panel del mod al menu de pausa
// ---------------------------------------------------------------------------

class $modify(PauseLayer) {

    void customSetup() {
        PauseLayer::customSetup();

        if (!PracticeReplayer::isButtonVisible()) return;

        // Panel principal
        auto winSize = CCDirector::get()->getWinSize();
        auto panel   = CCNode::create();
        panel->setPosition({winSize.width - 90.f, winSize.height - 60.f});

        // Fondo del panel
        auto bg = CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({150.f, 120.f});
        bg->setOpacity(200);
        panel->addChild(bg);

        // Titulo
        auto title = CCLabelBMFont::create("Practice\nReplayer", "goldFont.fnt");
        title->setScale(0.45f);
        title->setPosition({0.f, 42.f});
        panel->addChild(title);

        // Menu de botones
        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});

        // --- Boton GRABAR ---
        auto recLbl = CCLabelBMFont::create("Grabar", "bigFont.fnt");
        recLbl->setScale(0.5f);
        auto recBtn = CCMenuItemLabel::create(
            recLbl, this, menu_selector(PauseLayer_ExtraSetup::onRecord)
        );
        recBtn->setColor({255, 80, 80});
        recBtn->setPosition({0.f, 10.f});
        menu->addChild(recBtn);

        // --- Boton REPRODUCIR ---
        auto repLbl = CCLabelBMFont::create("Reproducir", "bigFont.fnt");
        repLbl->setScale(0.5f);
        auto repBtn = CCMenuItemLabel::create(
            repLbl, this, menu_selector(PauseLayer_ExtraSetup::onReplay)
        );
        repBtn->setColor({80, 200, 255});
        repBtn->setPosition({0.f, -10.f});
        menu->addChild(repBtn);

        // --- Boton GUARDAR ---
        auto saveLbl = CCLabelBMFont::create("Guardar", "bigFont.fnt");
        saveLbl->setScale(0.5f);
        auto saveBtn = CCMenuItemLabel::create(
            saveLbl, this, menu_selector(PauseLayer_ExtraSetup::onSave)
        );
        saveBtn->setColor({80, 255, 120});
        saveBtn->setPosition({0.f, -30.f});
        menu->addChild(saveBtn);

        // --- Toggle Activar/Desactivar Mod ---
        auto togLbl = CCLabelBMFont::create(
            PracticeReplayer::isModEnabled() ? "Mod: ON" : "Mod: OFF",
            "bigFont.fnt"
        );
        togLbl->setScale(0.5f);
        togLbl->setTag(99);
        auto togBtn = CCMenuItemLabel::create(
            togLbl, this, menu_selector(PauseLayer_ExtraSetup::onToggleMod)
        );
        togBtn->setPosition({0.f, -50.f});
        menu->addChild(togBtn);

        // --- Toggle Ocultar Boton ---
        auto hideLbl = CCLabelBMFont::create("Ocultar boton", "bigFont.fnt");
        hideLbl->setScale(0.4f);
        auto hideBtn = CCMenuItemLabel::create(
            hideLbl, this, menu_selector(PauseLayer_ExtraSetup::onToggleVisible)
        );
        hideBtn->setColor({200, 200, 200});
        hideBtn->setPosition({0.f, -67.f});
        menu->addChild(hideBtn);

        panel->addChild(menu);
        this->addChild(panel, 10);
    }

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

        // Actualiza el texto del boton
        if (auto btn = dynamic_cast<CCMenuItemLabel*>(sender)) {
            if (auto lbl = dynamic_cast<CCLabelBMFont*>(btn->getLabel())) {
                lbl->setString(nowEnabled ? "Mod: ON" : "Mod: OFF");
            }
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
};
