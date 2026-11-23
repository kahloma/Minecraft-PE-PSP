#include <pspctrl.h>
#include <pspgu.h>
#include <cstdio>
#include <cstring>
#include <array>

#include "client/gui/screens/menu.h"
#include "client/gui/screens/screen.h"
#include "client/gui/hud.h"
#include "gpu/sprite.h"
#include "gpu/gui_atlas.h"
#include "adhoc_bridge.h"

struct JoinScreen : Screen {
    void renderContent(MenuState& s);
    void handleInput(MenuState& s, unsigned int pressed, unsigned int held);
};

static const float ROW_H = 32.0f;
static const float PANEL_X = 10.0f;
static float panelY() { return MENU_BAR_H + 6.0f; }
static const float PANEL_W = VW - 20.0f;
static float panelH() { return (UI_HINTS_Y / UI_SCALE - 4.5f) - 3.0f - panelY(); }

static const char* kBarLabel0 = "Back";
static const char* kBarLabel1 = "Joinable";
static const char* kBarLabel2 = "Hosting";

static const char* getBarLabel(int i) {
    if (i == 0) return kBarLabel0;
    if (i == 1) return kBarLabel1;
    return kBarLabel2;
}

static float barButtonX(MenuState& s, int i) {
    if (i == 0) return 4.0f * MENU_PX;
    float hostW = menuBarButtonW(s, kBarLabel2);
    float hostX = VW - hostW - 4.0f * MENU_PX;
    if (i == 2) return hostX;
    return hostX - menuBarButtonW(s, kBarLabel1) - 4.0f * MENU_PX;
}

void joinListReset(MenuState& s) {
    s.joinUiRow = 1;
    s.joinBarSel = 1;
    s.joinRow = 0;
    s.joinScroll = 0.0f;
    s.joinEditMode = 0;
    g_adhoc.friendTab = 0;
}

void JoinScreen::handleInput(MenuState& s, unsigned int pressed, unsigned int ) {
    AppScreen& screen = s.screen;
    int totalRows = (g_adhoc.friendTab == 0) ? 1 : (s.worlds.count > 0 ? s.worlds.count : 1);

    if (s.joinRow >= totalRows) s.joinRow = totalRows - 1;
    if (s.joinRow < 0) s.joinRow = 0;

    if (pressed & PSP_CTRL_DOWN) {
        if (s.joinUiRow == 0) s.joinUiRow = 1;
        else if (s.joinUiRow == 1 && s.joinRow < totalRows - 1) s.joinRow++;
    }
    if (pressed & PSP_CTRL_UP) {
        if (s.joinUiRow == 1 && s.joinRow > 0) s.joinRow--;
        else if (s.joinUiRow == 1) s.joinUiRow = 0;
    }
    if (pressed & PSP_CTRL_RIGHT) {
        if (s.joinUiRow == 0 && s.joinBarSel < 2) {
            s.joinBarSel++;
            if (s.joinBarSel == 1) { g_adhoc.friendTab = 0; s.joinRow = 0; s.joinScroll = 0.0f; }
            if (s.joinBarSel == 2) { g_adhoc.friendTab = 1; s.joinRow = 0; s.joinScroll = 0.0f; }
        }
    }
    if (pressed & PSP_CTRL_LEFT) {
        if (s.joinUiRow == 0 && s.joinBarSel > 0) {
            s.joinBarSel--;
            if (s.joinBarSel == 1) { g_adhoc.friendTab = 0; s.joinRow = 0; s.joinScroll = 0.0f; }
        }
    }

    if (pressed & PSP_CTRL_CIRCLE)
        screen = SCREEN_TITLE;

    if (pressed & PSP_CTRL_CROSS) {
        if (s.joinUiRow == 0) {
            if (s.joinBarSel == 0) screen = SCREEN_TITLE;
            else if (s.joinBarSel == 1) { g_adhoc.friendTab = 0; s.joinRow = 0; s.joinScroll = 0.0f; s.joinUiRow = 1; }
            else if (s.joinBarSel == 2) { g_adhoc.friendTab = 1; s.joinRow = 0; s.joinScroll = 0.0f; s.joinUiRow = 1; }
        } else if (g_adhoc.friendTab == 0) {
            if (g_adhoc.hasHostWorld) {
                for (int i = s.worlds.count - 1; i >= 0; i--) {
                    if (std::strcmp(s.worlds.names[i], "_tmp_adhoc") == 0) {
                        worldListDelete(&s.worlds, i);
                    }
                }
                std::array<char, 64> created{};
                if (worldListCreate(&s.worlds, "_tmp_adhoc", created.data(),
                                    g_adhoc.hostMode, g_adhoc.hostSeed,
                                    g_adhoc.hostType, 0)) {
                    g_adhoc.isJoiner = true;
                    g_adhoc.isHosting = false;
                    snprintf(s.statusMsg, sizeof(s.statusMsg), "Joining %s...", g_adhoc.peerNick.data());
                    s.worldSelected = s.worlds.count - 1;
                    s.screen = SCREEN_GAME;
                }
            } else {
                snprintf(s.statusMsg, sizeof(s.statusMsg), "Scanning Ad-Hoc for friends...");
            }
        } else {
            if (s.worlds.count > 0 && s.joinRow < s.worlds.count) {
                g_adhoc.isHosting = true;
                g_adhoc.isJoiner = false;
                s.worldSelected = s.joinRow;
                snprintf(s.statusMsg, sizeof(s.statusMsg), "Hosting: %s", s.worlds.displayNames[s.joinRow]);
                s.screen = SCREEN_GAME;
            } else {
                createFormReset(s);
                g_adhoc.pendingHostCreate = true;
                s.screen = SCREEN_CREATE;
            }
        }
    }
}

void JoinScreen::renderContent(MenuState& s) {
    if (!s.haveTouch || !s.haveFont || !s.haveGui) return;
    int totalRows = (g_adhoc.friendTab == 0) ? 1 : (s.worlds.count > 0 ? s.worlds.count : 1);

    sceGuDisable(GU_DEPTH_TEST);
    const float pY = panelY(), pH = panelH();

    drawNinePatch(s, GA_SS_PANEL, 3.0f,
                  PANEL_X - 3.0f, pY - 3.0f, PANEL_W + 6.0f, pH + 6.0f);

    float contentH = totalRows * ROW_H;
    float selY = s.joinRow * ROW_H;
    float scroll = s.joinScroll;
    if (selY < scroll)               scroll = selY;
    if (selY + ROW_H > scroll + pH)  scroll = selY + ROW_H - pH;
    float maxScroll = contentH - pH; if (maxScroll < 0.0f) maxScroll = 0.0f;
    if (scroll > maxScroll) scroll = maxScroll;
    if (scroll < 0.0f) scroll = 0.0f;
    s.joinScroll = scroll;

    sceGuScissor((int)(PANEL_X * UI_SCALE), (int)(pY * UI_SCALE),
                 (int)(PANEL_W * UI_SCALE), (int)(pH * UI_SCALE));

    if (g_adhoc.friendTab == 0) {
        float rY = pY - scroll;
        bool hovered = (s.joinUiRow == 1 && s.joinRow == 0);
        guiTButton(s, PANEL_X, rY, PANEL_W, ROW_H, hovered, MENU_BEVEL);
        unsigned int titleCol = hovered ? 0xFFA0FFFFu : (g_adhoc.hasHostWorld ? 0xFF55FF55u : 0xFFFFFFFFu);
        std::array<char, 128> title{};
        std::array<char, 128> sub{};
        if (g_adhoc.hasHostWorld) {
            snprintf(title.data(), title.size(), "%s - %s", g_adhoc.peerNick.data(), g_adhoc.hostWorldName.data());
            snprintf(sub.data(), sub.size(), "Tap Cross (Z) to Join | Mode: %s | Seed: %ld",
                     g_adhoc.hostMode == 1 ? "Creative" : "Survival", g_adhoc.hostSeed);
        } else {
            snprintf(title.data(), title.size(), "No Joinable Friend Worlds Found");
            snprintf(sub.data(), sub.size(), "Have a friend select 'Hosting' tab to start a session");
        }
        fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 5.0f) * UI_SCALE,
                            title.data(), titleCol, UI_SCALE, PANEL_W - 10.0f);
        fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 16.0f) * UI_SCALE,
                            sub.data(), 0xFFBBBBBBu, UI_SCALE, PANEL_W - 10.0f);
    } else {
        if (s.worlds.count == 0) {
            float rY = pY - scroll;
            bool hovered = (s.joinUiRow == 1 && s.joinRow == 0);
            guiTButton(s, PANEL_X, rY, PANEL_W, ROW_H, hovered, MENU_BEVEL);
            fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 5.0f) * UI_SCALE,
                                "No Local Worlds - Create One First", 0xFFFFFFFFu, UI_SCALE, PANEL_W - 10.0f);
        } else {
            for (int i = 0; i < s.worlds.count; i++) {
                float rY = pY - scroll + i * ROW_H;
                if (rY > pY + pH || rY + ROW_H < pY) continue;
                bool hovered = (s.joinUiRow == 1 && s.joinRow == i);
                guiTButton(s, PANEL_X, rY, PANEL_W, ROW_H, hovered, MENU_BEVEL);
                unsigned int nameCol = hovered ? 0xFFA0FFFFu : 0xFFFFFFFFu;
                fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 5.0f) * UI_SCALE,
                                    s.worlds.displayNames[i], nameCol, UI_SCALE, PANEL_W - 10.0f);
                std::array<char, 128> sub{};
                snprintf(sub.data(), sub.size(), "Host via Ad-Hoc | %s",
                         s.worlds.gameModes[i] == 1 ? "Creative" : "Survival");
                fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 16.0f) * UI_SCALE,
                                    sub.data(), 0xFF55FF55u, UI_SCALE, PANEL_W - 10.0f);
            }
        }
    }
    sceGuScissor(0, 0, 480, 272);

    guiScrollbar((PANEL_X + PANEL_W - 2.0f) * UI_SCALE, pY * UI_SCALE, 2.0f * UI_SCALE,
                 pH * UI_SCALE, contentH * UI_SCALE, scroll * UI_SCALE);

    {
        float lb = 4.0f * MENU_PX + menuBarButtonW(s, kBarLabel0);
        float rb = barButtonX(s, 1);
        drawMenuHeader(s, "Friend Worlds", 0.0f, VW, MENU_BAR_H, MENU_BAR_TEXT, lb, rb - lb);
    }
    for (int i = 0; i < 3; i++) {
        bool tabActive = (i == 1 && g_adhoc.friendTab == 0) || (i == 2 && g_adhoc.friendTab == 1);
        bool on = (s.joinUiRow == 0 && s.joinBarSel == i) || tabActive;
        const char* lbl = getBarLabel(i);
        menuBarButton(s, barButtonX(s, i), menuBarButtonW(s, lbl), lbl, on);
    }

    sceGuEnable(GU_DEPTH_TEST);
}

static JoinScreen s_joinScreen;
Screen& joinScreen() { return s_joinScreen; }
