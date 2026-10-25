#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <string>
using namespace geode::prelude;

// Definidas en main.cpp
bool macroIsRecording();
bool macroIsPlaying();

static const char* kVert = R"(
attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)";

static const char* kFrag = R"(
precision mediump float;
varying vec2 v_uv;
uniform sampler2D u_tex;
uniform vec2 u_texel;
uniform float u_strength;
uniform float u_radius;
uniform float u_threshold;

vec3 bright(vec2 uv) {
    vec3 c = texture2D(u_tex, uv).rgb;
    float l = max(c.r, max(c.g, c.b));
    float k = max(l - u_threshold, 0.0) / max(1.0 - u_threshold, 0.001);
    return c * k;
}

void main() {
    vec2 s = u_texel * u_radius;
    vec3 g = bright(v_uv) * 4.0;
    g += bright(v_uv + vec2( s.x, 0.0)) * 2.0;
    g += bright(v_uv + vec2(-s.x, 0.0)) * 2.0;
    g += bright(v_uv + vec2(0.0,  s.y)) * 2.0;
    g += bright(v_uv + vec2(0.0, -s.y)) * 2.0;
    g += bright(v_uv + vec2( s.x,  s.y));
    g += bright(v_uv + vec2(-s.x,  s.y));
    g += bright(v_uv + vec2( s.x, -s.y));
    g += bright(v_uv + vec2(-s.x, -s.y));
    gl_FragColor = vec4(g * (u_strength / 16.0) * 3.0, 1.0);
}
)";

class BloomNode : public CCNode {
    GLuint m_bloomProg = 0;
    GLuint m_bloomTex = 0;
    int m_bloomW = 0;
    int m_bloomH = 0;
    GLint m_uTex = -1;
    GLint m_uTexel = -1;
    GLint m_uStrength = -1;
    GLint m_uRadius = -1;
    GLint m_uThreshold = -1;
    bool m_failed = false;

    GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    bool buildProgram() {
        GLuint vs = compile(GL_VERTEX_SHADER, kVert);
        GLuint fs = compile(GL_FRAGMENT_SHADER, kFrag);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return false;
        }
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glBindAttribLocation(p, kCCVertexAttrib_Position, "a_pos");
        glBindAttribLocation(p, kCCVertexAttrib_TexCoords, "a_uv");
        glLinkProgram(p);
        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            glDeleteProgram(p);
            return false;
        }
        m_bloomProg = p;
        m_uTex = glGetUniformLocation(p, "u_tex");
        m_uTexel = glGetUniformLocation(p, "u_texel");
        m_uStrength = glGetUniformLocation(p, "u_strength");
        m_uRadius = glGetUniformLocation(p, "u_radius");
        m_uThreshold = glGetUniformLocation(p, "u_threshold");
        return true;
    }

    void ensureTexture(int w, int h) {
        if (m_bloomTex && glIsTexture(m_bloomTex) && w == m_bloomW && h == m_bloomH) return;
        if (m_bloomTex) glDeleteTextures(1, &m_bloomTex);
        glGenTextures(1, &m_bloomTex);
        ccGLBindTexture2D(m_bloomTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_bloomW = w;
        m_bloomH = h;
    }

public:
    static BloomNode* create() {
        auto ret = new BloomNode();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    ~BloomNode() {
        if (m_bloomProg) glDeleteProgram(m_bloomProg);
        if (m_bloomTex) glDeleteTextures(1, &m_bloomTex);
    }

    void draw() override {
        auto mod = Mod::get();
        if (m_failed || !mod->getSettingValue<bool>("bloom-enabled")) return;

        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        int w = vp[2];
        int h = vp[3];
        if (w <= 0 || h <= 0) return;

        if (!m_bloomProg || !glIsProgram(m_bloomProg)) {
            m_bloomProg = 0;
            if (!buildProgram()) {
                m_failed = true;
                Notification::create(
                    "Bloom no disponible en este dispositivo",
                    NotificationIcon::Error
                )->show();
                return;
            }
        }
        ensureTexture(w, h);

        // copia la imagen actual a una textura
        ccGLBindTexture2D(m_bloomTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vp[0], vp[1], w, h);

        ccGLUseProgram(m_bloomProg);
        glUniform1i(m_uTex, 0);
        glUniform2f(m_uTexel, 1.f / w, 1.f / h);
        glUniform1f(m_uStrength, static_cast<float>(mod->getSettingValue<double>("bloom-strength")));
        glUniform1f(m_uRadius, static_cast<float>(mod->getSettingValue<int64_t>("bloom-radius")));
        glUniform1f(m_uThreshold, static_cast<float>(mod->getSettingValue<double>("bloom-threshold")));

        static const GLfloat verts[] = { -1.f, -1.f,  1.f, -1.f,  -1.f, 1.f,  1.f, 1.f };
        static const GLfloat uvs[]   = {  0.f,  0.f,  1.f,  0.f,   0.f, 1.f,  1.f, 1.f };

        // suma solo el resplandor encima de la imagen
        ccGLBlendFunc(GL_ONE, GL_ONE);
        ccGLEnableVertexAttribs(kCCVertexAttribFlag_Position | kCCVertexAttribFlag_TexCoords);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, 0, verts);
        glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, 0, uvs);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        ccGLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
};

// Texto pequeno en una esquina: estado de la ayuda y de las macros
class IndicatorNode : public CCNode {
    CCLabelBMFont* m_label = nullptr;
    int m_state = -1;

public:
    static IndicatorNode* create() {
        auto ret = new IndicatorNode();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        m_label = CCLabelBMFont::create("Precision: ON", "bigFont.fnt");
        m_label->setScale(0.3f);
        m_label->setAnchorPoint({0.f, 1.f});
        auto winSize = CCDirector::get()->getWinSize();
        m_label->setPosition({8.f, winSize.height - 6.f});
        this->addChild(m_label);
        this->scheduleUpdate();
        return true;
    }

    void update(float) override {
        auto mod = Mod::get();
        bool show = mod->getSavedValue<bool>("show-indicator", true);
        bool on = mod->getSettingValue<bool>("assist-enabled");
        bool rec = macroIsRecording();
        bool play = macroIsPlaying();
        int state = !show ? 0 : (1 + (on ? 1 : 0) + (rec ? 2 : 0) + (play ? 4 : 0));
        if (state == m_state) return;
        m_state = state;

        m_label->setVisible(state != 0);
        if (state == 0) return;

        std::string text = on ? "Precision: ON" : "Precision: OFF";
        ccColor3B color = on ? ccc3(120, 255, 140) : ccc3(200, 200, 200);
        if (rec) {
            text += "  REC";
            color = ccc3(255, 90, 90);
        } else if (play) {
            text += "  PLAY";
            color = ccc3(255, 220, 90);
        }
        m_label->setString(text.c_str());
        m_label->setColor(color);
        m_label->setOpacity(160);
    }
};

class $modify(BloomPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        if (auto bloom = BloomNode::create()) this->addChild(bloom, 100000);
        if (auto indicator = IndicatorNode::create()) this->addChild(indicator, 100001);
    }
};
