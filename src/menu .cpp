#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/GeodeUI.hpp>
using namespace geode::prelude;

class PrecisionMenu : public Popup {
protected:
    bool init() {
        if (!Popup::init(300.f, 250.f)) return false;
        this->setTitle("Precision");

        auto mod = Mod::get();
        this->addRow("Ayuda de clics", 75.f, 1,
                     mod->getSettingValue<bool>("assist-enabled"));
        this->addRow("Bloom (visual)", 48.f, 2,
                     mod->getSettingValue<bool>("bloom-enabled"));
        this->addRow("Indicador en pantalla", 21.f, 4,
                     mod->getSavedValue<bool>("show-indicator", true));
        this->addRow("Avisos de ayuda", -6.f, 3,
                     mod->getSavedValue<bool>("debug-toasts", false));

        auto presets = CCLabelBMFont::create("Presets de ayuda", "bigFont.fnt");
        presets->setScale(0.45f);
        m_mainLayer->addChildAtPosition(presets, Anchor::Center, {0.f, -34.f});

        this->addPreset("Suave", -108.f, 1);
        this->addPreset("Normal", -38.f, 2);
        this->addPreset("Fuerte", 34.f, 3);
        this->addPreset("Prueba", 106.f, 4);

        auto sprite = ButtonSprite::create("Mas ajustes");
        sprite->setScale(0.7f);
        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(PrecisionMenu::onMore)
        );
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {0.f, -98.f});
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

    void addPreset(const char* text, float x, int tag) {
        auto sprite = ButtonSprite::create(text);
        sprite->setScale(0.5f);
        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(PrecisionMenu::onPreset)
        );
        btn->setTag(tag);
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, -62.f});
    }

    void onToggle(CCObject* sender) {
        auto toggler = static_cast<CCMenuItemToggler*>(sender);
        bool on = !toggler->isToggled();
        switch (toggler->getTag()) {
            case 1: Mod::get()->setSettingValue<bool>("assist-enabled", on); break;
            case 2: Mod::get()->setSettingValue<bool>("bloom-enabled", on); break;
            case 3: Mod::get()->setSavedValue<bool>("debug-toasts", on); break;
            case 4: Mod::get()->setSavedValue<bool>("show-indicator", on); break;
        }
    }

    void onPreset(CCObject* sender) {
        int tag = static_cast<CCNode*>(sender)->getTag();
        int64_t power = 100, ground = 50, orb = 50, late = 33, robot = 100;
        const char* name = "Normal";
        switch (tag) {
            case 1:
                ground = 30; orb = 30; late = 16; robot = 80;
                name = "Suave";
                break;
            case 2:
                break;
            case 3:
                ground = 90; orb = 90; late = 60; robot = 120;
                name = "Fuerte";
                break;
            case 4:
                power = 300; ground = 150; orb = 150; late = 100; robot = 150;
                name = "Prueba";
                break;
        }
        auto mod = Mod::get();
        mod->setSettingValue<int64_t>("assist-power", power);
        mod->setSettingValue<int64_t>("ground-ms", ground);
        mod->setSettingValue<int64_t>("orb-ms", orb);
        mod->setSettingValue<int64_t>("late-ms", late);
        mod->setSettingValue<int64_t>("robot-hold-ms", robot);

        Notification::create(
            std::string("Preset: ") + name,
            NotificationIcon::Success, 1.f
        )->show();
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
