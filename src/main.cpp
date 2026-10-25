#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/utils/file.hpp>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>
using namespace geode::prelude;

// Definida en menu.cpp
void openPrecisionMenu();

// Se pone en true cuando la ayuda o una macro actua en este intento
static bool g_assistUsed = false;

// ---------- Macros ----------
struct MacroEvent {
    int step;
    bool down;
};

static std::vector<MacroEvent> g_macro;
static bool g_recording = false;    // el usuario activo Grabar
static bool g_playing = false;      // el usuario activo Reproducir
static bool g_recordActive = false; // grabando este intento (desde el reinicio)
static bool g_playActive = false;   // reproduciendo este intento
static size_t g_playIndex = 0;
static int g_steps = 0;             // pasos de fisica desde el inicio del intento

static std::filesystem::path macroPath() {
    return Mod::get()->getSaveDir() / "macro.txt";
}

void macroSetRecording(bool on) {
    g_recording = on;
    g_recordActive = false;
    if (on) {
        g_playing = false;
        g_playActive = false;
        g_macro.clear();
        Notification::create(
            "Grabando: reinicia el nivel para empezar",
            NotificationIcon::Success, 2.f
        )->show();
    }
}

void macroSetPlaying(bool on) {
    if (on && g_macro.empty()) {
        g_playing = false;
        g_playActive = false;
        Notification::create(
            "No hay macro en memoria", NotificationIcon::Warning, 2.f
        )->show();
        return;
    }
    g_playing = on;
    g_playActive = false;
    g_playIndex = 0;
    if (on) {
        g_recording = false;
        g_recordActive = false;
        Notification::create(
            "Reproduccion: reinicia el nivel para empezar",
            NotificationIcon::Success, 2.f
        )->show();
    }
}

bool macroIsRecording() { return g_recording; }
bool macroIsPlaying() { return g_playing; }
int macroEventCount() { return static_cast<int>(g_macro.size()); }

std::string macroSave() {
    std::string out = "# Precision macro v1\n# paso,accion\n";
    for (auto const& e : g_macro) {
        out += std::to_string(e.step);
        out += e.down ? ",down\n" : ",up\n";
    }
    auto res = geode::utils::file::writeString(macroPath(), out);
    if (res.isErr()) {
        return std::string("No se pudo guardar: ") + res.unwrapErr();
    }
    return std::string("Macro guardada en:\n") + macroPath().string();
}

std::string macroLoad() {
    auto res = geode::utils::file::readString(macroPath());
    if (res.isErr()) {
        return "Todavia no hay una macro guardada.";
    }
    std::vector<MacroEvent> loaded;
    std::istringstream in(res.unwrap());
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto comma = line.find(',');
        if (comma == std::string::npos) continue;
        int step = std::atoi(line.substr(0, comma).c_str());
        std::string action = line.substr(comma + 1);
        while (!action.empty() && (action.back() == '\r' || action.back() == ' ')) {
            action.pop_back();
        }
        if (action == "down") loaded.push_back(MacroEvent{step, true});
        else if (action == "up") loaded.push_back(MacroEvent{step, false});
    }
    g_macro = loaded;
    return std::string("Macro cargada: ") + std::to_string(loaded.size()) + " acciones.";
}

// ---------- Utilidades ----------
static double nowMs() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

// Aviso corto cuando la ayuda actua (se activa desde el menu)
static void debugToast(const char* msg) {
    if (Mod::get()->getSavedValue<bool>("debug-toasts", false)) {
        Notification::create(msg, NotificationIcon::Success, 0.4f)->show();
    }
}

// Cubo, bola, robot y arana necesitan ayuda; nave, UFO, onda y swing no
static bool needsAssist(PlayerObject* p) {
    return !(p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing);
}

static bool touchingRing(PlayerObject* p) {
    return p->m_touchingRings && p->m_touchingRings->count() > 0;
}

// El jugador puede actuar ahora: en el suelo o tocando un orbe
static bool canActNow(PlayerObject* p) {
    return p->m_isOnGround || touchingRing(p);
}

// ---------- Ayuda de clics, macros y velocidad ----------
class $modify(PrecisionLayer, GJBaseGameLayer) {
    struct Fields {
        bool pending = false;
        bool fingerDown = false;
        bool replaying = false;
        bool forceGround = false;
        bool wasInAir = false;
        bool clickedSinceGround = false;
        double groundUntil = 0.0;
        double orbUntil = 0.0;
        double releaseAt = 0.0;
        double leftGroundAt = 0.0;
    };

    // Velocidad lenta solo en modo practica (nunca mas rapido de lo normal)
    void update(float dt) {
        auto pl = PlayLayer::get();
        if (pl && pl->m_isPracticeMode) {
            double speed = Mod::get()->getSavedValue<double>("practice-speed", 1.0);
            if (speed < 1.0 && speed > 0.05) {
                dt = static_cast<float>(dt * speed);
            }
        }
        GJBaseGameLayer::update(dt);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        bool mine = button == 1 && isPlayer1 && !m_fields->replaying;

        // durante una macro se ignoran tus clics
        if (mine && g_playActive) return;

        if (mine) {
            m_fields->fingerDown = down;
            if (g_recordActive) g_macro.push_back(MacroEvent{g_steps, down});
        }

        if (mine && down && m_player1) {
            auto mod = Mod::get();
            double t = nowMs();
            bool assistOn = mod->getSettingValue<bool>("assist-enabled")
                && !g_recording && !g_playing;

            if (canActNow(m_player1)) {
                // clic normal: ya salto, no hay correccion tardia
                m_fields->clickedSinceGround = true;
            } else if (assistOn && needsAssist(m_player1)) {
                double power = static_cast<double>(
                    mod->getSettingValue<int64_t>("assist-power")
                ) / 100.0;
                double late = static_cast<double>(mod->getSettingValue<int64_t>("late-ms")) * power;
                double ground = static_cast<double>(mod->getSettingValue<int64_t>("ground-ms")) * power;
                double orb = static_cast<double>(mod->getSettingValue<int64_t>("orb-ms")) * power;

                bool lateOk = late > 0.0
                    && m_fields->wasInAir
                    && !m_fields->clickedSinceGround
                    && (t - m_fields->leftGroundAt) <= late;

                if (lateOk) {
                    // clic tardio: se aplica el salto como si siguieras en el suelo
                    m_fields->forceGround = true;
                    m_fields->clickedSinceGround = true;
                    g_assistUsed = true;
                    debugToast("Ayuda aplicada");
                } else if (ground > 0.0 || orb > 0.0) {
                    // clic temprano: se guarda unos ms
                    m_fields->pending = true;
                    m_fields->groundUntil = t + ground;
                    m_fields->orbUntil = t + orb;
                }
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        // reproducir macro: se aplican los clics de este paso
        if (g_playActive) {
            while (g_playIndex < g_macro.size() && g_macro[g_playIndex].step <= g_steps) {
                bool down = g_macro[g_playIndex].down;
                g_playIndex++;
                m_fields->replaying = true;
                GJBaseGameLayer::handleButton(down, 1, true);
                m_fields->replaying = false;
                g_assistUsed = true;
            }
        }

        bool forced = m_fields->forceGround && m_player1;
        if (forced) m_player1->m_isOnGround = true;

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        g_steps++;
        m_fields->forceGround = false;
        if (!m_player1) return;

        double t = nowMs();
        auto mod = Mod::get();
        bool on = mod->getSettingValue<bool>("assist-enabled")
            && !g_recording && !g_playing;

        // seguimiento del suelo / aire
        if (forced) {
            m_fields->wasInAir = true;
            m_fields->clickedSinceGround = true;
        } else if (m_player1->m_isOnGround) {
            if (m_fields->wasInAir) m_fields->clickedSinceGround = false;
            m_fields->wasInAir = false;
        } else if (!m_fields->wasInAir) {
            m_fields->wasInAir = true;
            m_fields->leftGroundAt = t;
            // si saltaste por mantener el dedo, no cuenta como caida de plataforma
            if (m_fields->fingerDown) m_fields->clickedSinceGround = true;
        }

        // soltar el clic ayudado
        if (m_fields->releaseAt > 0.0 && t >= m_fields->releaseAt) {
            m_fields->releaseAt = 0.0;
            if (!m_fields->fingerDown) {
                m_fields->replaying = true;
                GJBaseGameLayer::handleButton(false, 1, true);
                m_fields->replaying = false;
            }
        }

        // clic temprano guardado
        if (!m_fields->pending) return;

        if (!on || !needsAssist(m_player1)) {
            m_fields->pending = false;
            return;
        }

        bool hitGround = t <= m_fields->groundUntil && m_player1->m_isOnGround;
        bool hitOrb = t <= m_fields->orbUntil && touchingRing(m_player1);

        if (hitGround || hitOrb) {
            m_fields->pending = false;
            if (!m_fields->fingerDown) {
                m_fields->replaying = true;
                GJBaseGameLayer::handleButton(true, 1, true);
                m_fields->replaying = false;
                m_fields->clickedSinceGround = true;
                g_assistUsed = true;
                debugToast("Ayuda aplicada");
                double robotHold = static_cast<double>(
                    mod->getSettingValue<int64_t>("robot-hold-ms")
                );
                m_fields->releaseAt = t + (m_player1->m_isRobot ? robotHold : 30.0);
            }
        } else if (t > m_fields->groundUntil && t > m_fields->orbUntil) {
            m_fields->pending = false;
        }
    }
};

// ---------- Reinicio de intento y fin de nivel ----------
class $modify(PrecisionPlay, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        g_assistUsed = false;
        g_steps = 0;
        g_playIndex = 0;
        g_recordActive = g_recording;
        g_playActive = g_playing && !g_macro.empty();
        if (g_recordActive) g_macro.clear();
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        if (g_recordActive && !g_macro.empty()) {
            Notification::create(macroSave(), NotificationIcon::Success, 4.f)->show();
        }
    }
};

// ---------- No guardar progreso si se uso ayuda ----------
class $modify(PrecisionLevel, GJGameLevel) {
    void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
        if (g_assistUsed && !isPracticeMode) {
            Notification::create(
                "Precision: progreso no guardado (ayuda usada)",
                NotificationIcon::Warning, 3.f
            )->show();
            return;
        }
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};

// ---------- Boton en el menu de pausa ----------
class $modify(PrecisionPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto winSize = CCDirector::get()->getWinSize();

        auto sprite = ButtonSprite::create("Precision");
        sprite->setScale(0.6f);
        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(PrecisionPause::onPrecision)
        );

        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({winSize.width - 60.f, winSize.height - 25.f});
        this->addChild(menu);
    }

    void onPrecision(CCObject*) {
        openPrecisionMenu();
    }
};
