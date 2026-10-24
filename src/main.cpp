#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/ui/GeodeUI.hpp>
using namespace geode::prelude;

// Se pone en true cuando la ayuda aplica un clic en este intento
static bool g_assistUsed = false;

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
        int groundLeft = 0;
        int orbLeft = 0;
        int releaseIn = 0;
    };

    void handleButton(bool down, int button, bool isPlayer1) {
        bool mine = button == 1 && isPlayer1 && !m_fields->replaying;
        if (mine) m_fields->fingerDown = down;

        auto mod = Mod::get();
        int ground = static_cast<int>(mod->getSettingValue<int64_t>("ground-frames"));
        int orb = static_cast<int>(mod->getSettingValue<int64_t>("orb-frames"));
        bool on = mod->getSettingValue<bool>("assist-enabled");

        if (mine && down && on && (ground > 0 || orb > 0) && m_player1
            && needsAssist(m_player1) && !canActNow(m_player1)) {
            m_fields->pending = true;
            m_fields->groundLeft = ground;
            m_fields->orbLeft = orb;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (m_fields->releaseIn > 0 && --m_fields->releaseIn == 0 && !m_fields->fingerDown) {
            m_fields->replaying = true;
            GJBaseGameLayer::handleButton(false, 1, true);
            m_fields->replaying = false;
        }

        if (!m_fields->pending || !m_player1) return;

        if (!Mod::get()->getSettingValue<bool>("assist-enabled")
            || !needsAssist(m_player1)) {
            m_fields->pending = false;
            return;
        }

        bool hitGround = m_fields->groundLeft > 0 && m_player1->m_isOnGround;
        bool hitOrb = m_fields->orbLeft > 0 && touchingRing(m_player1);

        if (hitGround || hitOrb) {
            m_fields->pending = false;
            if (!m_fields->fingerDown) {
                m_fields->replaying = true;
                GJBaseGameLayer::handleButton(true, 1, true);
                m_fields->replaying = false;
                g_assistUsed = true;
                int robotHold = static_cast<int>(
                    Mod::get()->getSettingValue<int64_t>("robot-hold-frames")
                );
                m_fields->releaseIn = m_player1->m_isRobot ? robotHold : 2;
            }
        } else {
            if (m_fields->groundLeft > 0) m_fields->groundLeft--;
            if (m_fields->orbLeft > 0) m_fields->orbLeft--;
            if (m_fields->groundLeft <= 0 && m_fields->orbLeft <= 0) {
                m_fields->pending = false;
            }
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
        openSettingsPopup(Mod::get());
    }
};
