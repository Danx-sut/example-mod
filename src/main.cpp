#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
using namespace geode::prelude;

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
        int framesLeft = 0;
        int releaseIn = 0;
    };

    void handleButton(bool down, int button, bool isPlayer1) {
        bool mine = button == 1 && isPlayer1 && !m_fields->replaying;
        if (mine) m_fields->fingerDown = down;

        auto max = Mod::get()->getSettingValue<int64_t>("buffer-frames");
        bool on = Mod::get()->getSettingValue<bool>("assist-enabled");
        if (mine && down && on && max > 0 && m_player1
            && needsAssist(m_player1) && !canActNow(m_player1)) {
            m_fields->pending = true;
            m_fields->framesLeft = static_cast<int>(max);
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

        if (canActNow(m_player1)) {
            m_fields->pending = false;
            if (!m_fields->fingerDown) {
                m_fields->replaying = true;
                GJBaseGameLayer::handleButton(true, 1, true);
                m_fields->replaying = false;
                // el robot salta mas alto si se mantiene mas tiempo
                m_fields->releaseIn = m_player1->m_isRobot ? 8 : 2;
            }
        } else if (--m_fields->framesLeft <= 0) {
            m_fields->pending = false;
        }
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
