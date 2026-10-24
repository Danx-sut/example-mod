#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
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
        if (m_fields->replaying || button != 1 || !isPlayer1) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }
        m_fields->fingerDown = down;
        auto max = Mod::get()->getSettingValue<int64_t>("buffer-frames");
        if (down && max > 0 && m_player1 && !m_player1->m_isOnGround) {
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
