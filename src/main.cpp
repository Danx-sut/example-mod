#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/GeodeUI.hpp>
using namespace geode::prelude;

class PrecisionMenu : public Popup {
protected:
    bool init() {
        if (!Popup::init(300.f, 200.f)) return false;
        this->setTitle("Precision");

        auto mod = Mod::get();
        this->addRow("Ayuda de clics", 45.f, 1,
                     mod->getSettingValue<bool>("assist-enabled"));
        this->addRow("Bloom (visual)", 10.f, 2,
                     mod->getSettingValue<bool>("bloom-enabled"));
        this->addRow("Avisos de ayuda", -25.f, 3,
                     mod->getSavedValue<bool>("debug-toasts", false));

        auto sprite = ButtonSprite::create("Mas ajustes");
        sprite->setScale(0.7f);
        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(PrecisionMenu::onMore)
        );
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {0.f, -65.f});
        return true;
    }

    void addRow(const char* text, float y, int tag, bool value) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setScale(0.5f);
        label->setAnchorPoint({0.f, 0.5f});
        m_mainLayer->addChildAtPosition(label, Anchor::Center, {-120.f, y});

        auto toggler = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(PrecisionMenu::onToggle), 0.7f
        );
        toggler->setTag(tag);
        toggler->toggle(value);
        m_buttonMenu->addChildAtPosition(toggler, Anchor::Center, {100.f, y});
    }

    void onToggle(CCObject* sender) {
        auto toggler = static_cast<CCMenuItemToggler*>(sender);
        bool on = !toggler->isToggled();
        switch (toggler->getTag()) {
            case 1: Mod::get()->setSettingValue<bool>("assist-enabled", on); break;
            case 2: Mod::get()->setSettingValue<bool>("bloom-enabled", on); break;
            case 3: Mod::get()->setSavedValue<bool>("debug-toasts", on); break;
        }
    }

    void onMore(CCObject*) {
        openSettingsPopup(Mod::get());
    }

public:
    static PrecisionMenu* create() {
        auto ret = new PrecisionMenu();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

void openPrecisionMenu() {
    if (auto menu = PrecisionMenu::create()) menu->show();
}
