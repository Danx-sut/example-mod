#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
using namespace geode::prelude;

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
        if (mine && down && on && max > 0 && m_player1 && !m_player1->m_isOnGround) {
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

        if (!Mod::get()->getSettingValue<bool>("assist-enabled")) {
            m_fields->pending = false;
            return;
        }

        if (m_player1->m_isOnGround) {
            m_fields->pending = false;
            if (!m_fields->fingerDown) {
                m_fields->replaying = true;
                GJBaseGameLayer::handleButton(true, 1, true);
                m_fields->replaying = false;
                m_fields->releaseIn = 2;
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
