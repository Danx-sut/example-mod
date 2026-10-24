#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <chrono>
using namespace geode::prelude;

// Definida en menu.cpp
void openPrecisionMenu();

// Se pone en true cuando la ayuda actua en este intento
static bool g_assistUsed = false;

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

    void handleButton(bool down, int button, bool isPlayer1) {
        bool mine = button == 1 && isPlayer1 && !m_fields->replaying;
        if (mine) m_fields->fingerDown = down;

        if (mine && down && m_player1) {
            auto mod = Mod::get();
            double t = nowMs();

            if (canActNow(m_player1)) {
                // clic normal: ya salto, no hay correccion tardia
                m_fields->clickedSinceGround = true;
            } else if (mod->getSettingValue<bool>("assist-enabled")
                       && needsAssist(m_player1)) {
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
        bool forced = m_fields->forceGround && m_player1;
        if (forced) m_player1->m_isOnGround = true;

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        m_fields->forceGround = false;
        if (!m_player1) return;

        double t = nowMs();
        auto mod = Mod::get();
        bool on = mod->getSettingValue<bool>("assist-enabled");

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

// Cada intento nuevo empieza sin marca de ayuda
class $modify(PrecisionPlay, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        g_assistUsed = false;
    }
};

// Si se uso la ayuda, no se guarda el progreso normal (la practica si)
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

// Boton en el menu de pausa que abre el menu de Precision
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
