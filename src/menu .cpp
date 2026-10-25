#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <algorithm>
#include <string>
#include <vector>
using namespace geode::prelude;

// Definidas en main.cpp
void macroSetRecording(bool on);
void macroSetPlaying(bool on);
bool macroIsRecording();
bool macroIsPlaying();
int macroEventCount();
std::string macroSave();
std::string macroLoad();

static const float kMenuScales[] = {0.7f, 0.85f, 1.0f, 1.15f};
static const double kSpeeds[] = {1.0, 0.75, 0.5, 0.25};

// Etapas de ayuda 1..7 (la 8 es "sin ayuda"), en milisegundos
struct Stage {
    int64_t ground;
    int64_t orb;
    int64_t late;
    int64_t robot;
};

static const Stage kStages[7] = {
    {15, 15, 8, 60},
    {30, 30, 16, 80},
    {50, 50, 33, 100},
    {70, 70, 45, 110},
    {90, 90, 60, 120},
    {120, 120, 80, 130},
    {150, 150, 100, 150},
};

static std::string percentText(const char* prefix, double v) {
    return std::string(prefix) + std::to_string(static_cast<int>(v * 100.0 + 0.5)) + "%";
}

class PrecisionMenu : public Popup {
protected:
    std::vector<CCNode*> m_pageNodes;
    ButtonSprite* m_sizeSpr = nullptr;
    ButtonSprite* m_speedSpr = nullptr;
    ButtonSprite* m_recSpr = nullptr;
    ButtonSprite* m_playSpr = nullptr;
    CCLabelBMFont* m_countLbl = nullptr;
    CCMenuItemToggler* m_assistToggler = nullptr;
    CCLabelBMFont* m_stageLbl = nullptr;
    CCLabelBMFont* m_stageInfo = nullptr;

    bool init() {
        if (!Popup::init(300.f, 250.f)) return false;
        this->setTitle("Precision");

        const char* names[4] = {"Ayuda", "Visual", "Practica", "Macros"};
        for (int i = 0; i < 4; i++) {
            auto spr = ButtonSprite::create(names[i]);
            spr->setScale(0.45f);
            auto btn = CCMenuItemSpriteExtra::create(
                spr, this, menu_selector(PrecisionMenu::onTab)
            );
            btn->setTag(i);
            m_buttonMenu->addChildAtPosition(
                btn, Anchor::Center, {-105.f + i * 70.f, 82.f}
            );
        }

        this->applyScale();
        this->showPage(0);
        return true;
    }

    // ----- ayudas para armar las paginas -----
    void track(CCNode* n) { m_pageNodes.push_back(n); }

    CCLabelBMFont* addLabel(const char* text, float x, float y, float scale, bool left) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setScale(scale);
        if (left) label->setAnchorPoint({0.f, 0.5f});
        m_mainLayer->addChildAtPosition(label, Anchor::Center, {x, y});
        this->track(label);
        return label;
    }

    ButtonSprite* addButton(const char* text, float x, float y, int tag,
                            SEL_MenuHandler cb, float scale) {
        auto spr = ButtonSprite::create(text);
        spr->setScale(scale);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        btn->setTag(tag);
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, y});
        this->track(btn);
        return spr;
    }

    CCMenuItemToggler* addToggleRow(const char* text, float y, int tag, bool value) {
        this->addLabel(text, -120.f, y, 0.5f, true);
        auto toggler = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(PrecisionMenu::onToggle), 0.7f
        );
        toggler->setTag(tag);
        toggler->toggle(value);
        m_buttonMenu->addChildAtPosition(toggler, Anchor::Center, {100.f, y});
        this->track(toggler);
        return toggler;
    }

    int speedIndex() {
        int i = static_cast<int>(Mod::get()->getSavedValue<int64_t>("practice-speed-idx", 0));
        return (i < 0 || i > 3) ? 0 : i;
    }

    int sizeIndex() {
        int i = static_cast<int>(Mod::get()->getSavedValue<int64_t>("menu-scale-idx", 2));
        return (i < 0 || i > 3) ? 2 : i;
    }

    void applyScale() {
        m_mainLayer->setScale(kMenuScales[this->sizeIndex()]);
    }

    // ----- etapas de ayuda -----
    // 1..7 = etapa, 8 = sin ayuda, 0 = valores propios
    int currentStage() {
        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("assist-enabled")) return 8;
        if (mod->getSettingValue<int64_t>("assist-power") != 100) return 0;
        int64_t g = mod->getSettingValue<int64_t>("ground-ms");
        int64_t o = mod->getSettingValue<int64_t>("orb-ms");
        int64_t l = mod->getSettingValue<int64_t>("late-ms");
        int64_t r = mod->getSettingValue<int64_t>("robot-hold-ms");
        for (int i = 0; i < 7; i++) {
            if (kStages[i].ground == g && kStages[i].orb == o
                && kStages[i].late == l && kStages[i].robot == r) {
                return i + 1;
            }
        }
        return 0;
    }

    void applyStage(int s) {
        auto mod = Mod::get();
        if (s >= 8) {
            mod->setSettingValue<bool>("assist-enabled", false);
            return;
        }
        auto const& st = kStages[s - 1];
        mod->setSettingValue<bool>("assist-enabled", true);
        mod->setSettingValue<int64_t>("assist-power", 100);
        mod->setSettingValue<int64_t>("ground-ms", st.ground);
        mod->setSettingValue<int64_t>("orb-ms", st.orb);
        mod->setSettingValue<int64_t>("late-ms", st.late);
        mod->setSettingValue<int64_t>("robot-hold-ms", st.robot);
    }

    void refreshStage(bool syncToggle) {
        int s = this->currentStage();

        if (syncToggle && m_assistToggler) {
            m_assistToggler->toggle(Mod::get()->getSettingValue<bool>("assist-enabled"));
        }

        if (m_stageLbl) {
            std::string txt = s == 8 ? "OFF" : (s == 0 ? "Propia" : std::to_string(s) + "/7");
            m_stageLbl->setString(txt.c_str());
        }
        if (m_stageInfo) {
            std::string info;
            if (s >= 1 && s <= 7) {
                auto const& st = kStages[s - 1];
                info = "Suelo " + std::to_string(st.ground)
                     + " ms | Orbes " + std::to_string(st.orb)
                     + " ms | Tardio " + std::to_string(st.late) + " ms";
            } else if (s == 8) {
                info = "Sin ayuda";
            } else {
                info = "Valores propios (ver Mas ajustes)";
            }
            m_stageInfo->setString(info.c_str());
        }
    }

    void refreshMacro() {
        if (m_recSpr) {
            m_recSpr->setString(macroIsRecording() ? "Grabar: ON" : "Grabar: OFF");
        }
        if (m_playSpr) {
            m_playSpr->setString(macroIsPlaying() ? "Reproducir: ON" : "Reproducir: OFF");
        }
        if (m_countLbl) {
            m_countLbl->setString(
                (std::string("Acciones en memoria: ")
                 + std::to_string(macroEventCount())).c_str()
            );
        }
    }

    // ----- paginas -----
    void showPage(int p) {
        for (auto n : m_pageNodes) n->removeFromParent();
        m_pageNodes.clear();
        m_sizeSpr = nullptr;
        m_speedSpr = nullptr;
        m_recSpr = nullptr;
        m_playSpr = nullptr;
        m_countLbl = nullptr;
        m_assistToggler = nullptr;
        m_stageLbl = nullptr;
        m_stageInfo = nullptr;

        auto mod = Mod::get();

        if (p == 0) {
            m_assistToggler = this->addToggleRow(
                "Ayuda de clics", 50.f, 1, mod->getSettingValue<bool>("assist-enabled")
            );
            this->addToggleRow("Avisos de ayuda", 24.f, 3,
                               mod->getSavedValue<bool>("debug-toasts", false));

            this->addLabel("Etapa", -120.f, -4.f, 0.5f, true);
            this->addButton("-", 30.f, -4.f, -1, menu_selector(PrecisionMenu::onStage), 0.5f);
            m_stageLbl = this->addLabel("", 70.f, -4.f, 0.5f, false);
            this->addButton("+", 110.f, -4.f, 1, menu_selector(PrecisionMenu::onStage), 0.5f);
            m_stageInfo = this->addLabel("", 0.f, -28.f, 0.35f, false);
            this->refreshStage(false);

            this->addButton("Guardar mio", -100.f, -58.f, 1, menu_selector(PrecisionMenu::onCustom), 0.5f);
            this->addButton("Cargar mio", 0.f, -58.f, 2, menu_selector(PrecisionMenu::onCustom), 0.5f);
            this->addButton("Prueba", 100.f, -58.f, 0, menu_selector(PrecisionMenu::onTest), 0.5f);
            this->addButton("Mas ajustes", 0.f, -95.f, 0, menu_selector(PrecisionMenu::onMore), 0.7f);
        } else if (p == 1) {
            this->addToggleRow("Bloom (visual)", 45.f, 2,
                               mod->getSettingValue<bool>("bloom-enabled"));
            this->addToggleRow("Indicador en pantalla", 20.f, 4,
                               mod->getSavedValue<bool>("show-indicator", true));
            m_sizeSpr = this->addButton(
                percentText("Tamano del menu: ", kMenuScales[this->sizeIndex()]).c_str(),
                0.f, -15.f, 0, menu_selector(PrecisionMenu::onSize), 0.6f
            );
        } else if (p == 2) {
            m_speedSpr = this->addButton(
                percentText("Velocidad: ", kSpeeds[this->speedIndex()]).c_str(),
                0.f, 30.f, 0, menu_selector(PrecisionMenu::onSpeed), 0.7f
            );
            this->addLabel("Solo funciona en modo practica", 0.f, -5.f, 0.4f, false);
        } else {
            m_recSpr = this->addButton(
                macroIsRecording() ? "Grabar: ON" : "Grabar: OFF",
                0.f, 48.f, 0, menu_selector(PrecisionMenu::onRecord), 0.7f
            );
            m_playSpr = this->addButton(
                macroIsPlaying() ? "Reproducir: ON" : "Reproducir: OFF",
                0.f, 14.f, 0, menu_selector(PrecisionMenu::onPlay), 0.7f
            );
            this->addButton("Guardar", -60.f, -22.f, 0, menu_selector(PrecisionMenu::onMacroSave), 0.6f);
            this->addButton("Cargar", 60.f, -22.f, 0, menu_selector(PrecisionMenu::onMacroLoad), 0.6f);
            m_countLbl = this->addLabel(
                (std::string("Acciones en memoria: ")
                 + std::to_string(macroEventCount())).c_str(),
                0.f, -52.f, 0.4f, false
            );
            this->addLabel("Graba desde el inicio del nivel", 0.f, -72.f, 0.35f, false);
        }
    }

    // ----- botones -----
    void onTab(CCObject* sender) {
        this->showPage(static_cast<CCNode*>(sender)->getTag());
    }

    void onToggle(CCObject* sender) {
        auto toggler = static_cast<CCMenuItemToggler*>(sender);
        bool on = !toggler->isToggled();
        switch (toggler->getTag()) {
            case 1:
                Mod::get()->setSettingValue<bool>("assist-enabled", on);
                this->refreshStage(false);
                break;
            case 2: Mod::get()->setSettingValue<bool>("bloom-enabled", on); break;
            case 3: Mod::get()->setSavedValue<bool>("debug-toasts", on); break;
            case 4: Mod::get()->setSavedValue<bool>("show-indicator", on); break;
        }
    }

    void onStage(CCObject* sender) {
        int dir = static_cast<CCNode*>(sender)->getTag();
        int s = this->currentStage();
        if (s == 0) s = 3; // desde valores propios se parte de la etapa 3
        s = std::max(1, std::min(8, s + dir));
        this->applyStage(s);
        this->refreshStage(true);
    }

    void onTest(CCObject*) {
        auto mod = Mod::get();
        mod->setSettingValue<bool>("assist-enabled", true);
        mod->setSettingValue<int64_t>("assist-power", 300);
        mod->setSettingValue<int64_t>("ground-ms", 150);
        mod->setSettingValue<int64_t>("orb-ms", 150);
        mod->setSettingValue<int64_t>("late-ms", 100);
        mod->setSettingValue<int64_t>("robot-hold-ms", 150);
        this->refreshStage(true);
        Notification::create(
            "Prueba: ventanas al maximo", NotificationIcon::Success, 1.f
        )->show();
    }

    void onCustom(CCObject* sender) {
        int tag = static_cast<CCNode*>(sender)->getTag();
        auto mod = Mod::get();
        if (tag == 1) {
            mod->setSavedValue<int64_t>("custom-power", mod->getSettingValue<int64_t>("assist-power"));
            mod->setSavedValue<int64_t>("custom-ground", mod->getSettingValue<int64_t>("ground-ms"));
            mod->setSavedValue<int64_t>("custom-orb", mod->getSettingValue<int64_t>("orb-ms"));
            mod->setSavedValue<int64_t>("custom-late", mod->getSettingValue<int64_t>("late-ms"));
            mod->setSavedValue<int64_t>("custom-robot", mod->getSettingValue<int64_t>("robot-hold-ms"));
            mod->setSavedValue<bool>("custom-saved", true);
            Notification::create("Preset propio guardado", NotificationIcon::Success, 1.f)->show();
        } else {
            if (!mod->getSavedValue<bool>("custom-saved", false)) {
                Notification::create("Aun no guardaste un preset propio", NotificationIcon::Warning, 1.5f)->show();
                return;
            }
            mod->setSettingValue<int64_t>("assist-power", mod->getSavedValue<int64_t>("custom-power", 100));
            mod->setSettingValue<int64_t>("ground-ms", mod->getSavedValue<int64_t>("custom-ground", 50));
            mod->setSettingValue<int64_t>("orb-ms", mod->getSavedValue<int64_t>("custom-orb", 50));
            mod->setSettingValue<int64_t>("late-ms", mod->getSavedValue<int64_t>("custom-late", 33));
            mod->setSettingValue<int64_t>("robot-hold-ms", mod->getSavedValue<int64_t>("custom-robot", 100));
            this->refreshStage(true);
            Notification::create("Preset propio cargado", NotificationIcon::Success, 1.f)->show();
        }
    }

    void onSize(CCObject*) {
        int i = (this->sizeIndex() + 1) % 4;
        Mod::get()->setSavedValue<int64_t>("menu-scale-idx", i);
        this->applyScale();
        if (m_sizeSpr) {
            m_sizeSpr->setString(percentText("Tamano del menu: ", kMenuScales[i]).c_str());
        }
    }

    void onSpeed(CCObject*) {
        int i = (this->speedIndex() + 1) % 4;
        Mod::get()->setSavedValue<int64_t>("practice-speed-idx", i);
        Mod::get()->setSavedValue<double>("practice-speed", kSpeeds[i]);
        if (m_speedSpr) {
            m_speedSpr->setString(percentText("Velocidad: ", kSpeeds[i]).c_str());
        }
    }

    void onRecord(CCObject*) {
        macroSetRecording(!macroIsRecording());
        this->refreshMacro();
    }

    void onPlay(CCObject*) {
        macroSetPlaying(!macroIsPlaying());
        this->refreshMacro();
    }

    void onMacroSave(CCObject*) {
        if (macroEventCount() == 0) {
            FLAlertLayer::create("Macro", "No hay nada que guardar todavia.", "OK")->show();
            return;
        }
        FLAlertLayer::create("Macro", macroSave(), "OK")->show();
    }

    void onMacroLoad(CCObject*) {
        std::string msg = macroLoad();
        this->refreshMacro();
        FLAlertLayer::create("Macro", msg, "OK")->show();
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
