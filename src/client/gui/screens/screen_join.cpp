#include <pspctrl.h>
#include <pspgu.h>
#include <cstdio>

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

static const char* kBarLabels[3] = { "Back", "External", "Edit" };
static float barButtonX(MenuState& s, int i) {
    if (i == 0) return 4.0f * MENU_PX;
    float editW = menuBarButtonW(s, kBarLabels[2]);
    float editX = VW - editW - 4.0f * MENU_PX;
    if (i == 2) return editX;
    return editX - menuBarButtonW(s, kBarLabels[1]) - 4.0f * MENU_PX;
}

void joinListReset(MenuState& s) {
    externalServersLoad(&s.servers);
    s.joinUiRow = 1;
    s.joinBarSel = 0;
    s.joinRow = 0;
    s.joinScroll = 0.0f;
    s.joinEditMode = 0;
}

void JoinScreen::handleInput(MenuState& s, unsigned int pressed, unsigned int ) {
    ExternalServerList& list = s.servers;
    AppScreen& screen = s.screen;
    int totalRows = list.count + 1; // Row 0 = PSP Ad-Hoc Host

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
        if (s.joinUiRow == 0 && s.joinBarSel < 2) s.joinBarSel++;
    }
    if (pressed & PSP_CTRL_LEFT) {
        if (s.joinUiRow == 0 && s.joinBarSel > 0) s.joinBarSel--;
    }

    if (pressed & PSP_CTRL_CIRCLE)
        screen = SCREEN_TITLE;

    if (pressed & PSP_CTRL_CROSS) {
        if (s.joinUiRow == 0) {
            if (s.joinBarSel == 0) screen = SCREEN_TITLE;
            else if (s.joinBarSel == 1) { addServerFormReset(s); screen = SCREEN_ADD_SERVER; }
            else s.joinEditMode = !s.joinEditMode;
        } else if (s.joinRow == 0) {
            if (g_adhoc.hasHostWorld) {
                char created[64];
                if (worldListCreate(&s.worlds, "AdHoc_World", created,
                                    g_adhoc.hostMode, g_adhoc.hostSeed,
                                    g_adhoc.hostType, 0)) {
                    g_adhoc.isJoiner = true;
                    snprintf(s.statusMsg, sizeof(s.statusMsg), "Joining Ad-Hoc World...");
                    s.worldSelected = s.worlds.count - 1;
                    s.screen = SCREEN_GAME;
                }
            } else {
                snprintf(s.statusMsg, sizeof(s.statusMsg), "Waiting for PSP Host world...");
            }
        } else if (s.joinRow - 1 < list.count) {
            int idx = s.joinRow - 1;
            const ExternalServer& sv = list.servers[idx];
            if (s.joinEditMode) {
                externalServersRemove(&list, idx);
                if (s.joinRow >= list.count + 1) s.joinRow = list.count;
            } else {
                snprintf(s.statusMsg, sizeof(s.statusMsg), "Joining: %s (%s:%d)",
                         sv.name, sv.addr, sv.port);
                screen = SCREEN_TITLE;
            }
        }
    }
}

void JoinScreen::renderContent(MenuState& s) {
    if (!s.haveTouch || !s.haveFont || !s.haveGui) return;
    ExternalServerList& list = s.servers;
    int totalRows = list.count + 1;

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

    // Row 0: Live PSP Ad-Hoc Local Session
    {
        float rY = pY - scroll;
        bool hovered = (s.joinUiRow == 1 && s.joinRow == 0);
        guiTButton(s, PANEL_X, rY, PANEL_W, ROW_H, hovered, MENU_BEVEL);
        unsigned int titleCol = hovered ? 0xFFA0FFFFu : (g_adhoc.hasHostWorld ? 0xFF55FF55u : 0xFFFFFFFFu);
        const char* title = g_adhoc.hasHostWorld ? "PSP Ad-Hoc World (Ready to Join)" : "PSP Ad-Hoc Local (Scanning...)";
        char sub[80];
        if (g_adhoc.hasHostWorld) {
            snprintf(sub, sizeof(sub), "Seed: %ld | Mode: %s",
                     g_adhoc.hostSeed, g_adhoc.hostMode == 1 ? "Creative" : "Survival");
        } else {
            snprintf(sub, sizeof(sub), "Start a world on Host PSP to join automatically");
        }
        fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 5.0f) * UI_SCALE,
                            title, titleCol, UI_SCALE, PANEL_W - 10.0f);
        fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 16.0f) * UI_SCALE,
                            sub, 0xFFBBBBBBu, UI_SCALE, PANEL_W - 10.0f);
    }

    for (int i = 0; i < list.count; i++) {
        const ExternalServer& sv = list.servers[i];
        int rowIdx = i + 1;
        float rY = pY - scroll + rowIdx * ROW_H;
        if (rY > pY + pH || rY + ROW_H < pY) continue;

        bool hovered = (s.joinUiRow == 1 && s.joinRow == rowIdx);
        guiTButton(s, PANEL_X, rY, PANEL_W, ROW_H, hovered, MENU_BEVEL);

        float textW = PANEL_W - 10.0f;
        if (s.joinEditMode) {
            float bs = ROW_H - 8.0f;
            float bx = PANEL_X + PANEL_W - bs - 4.0f;
            float by = rY + (ROW_H - bs) / 2.0f;
            guiTButton(s, bx, by, bs, bs, hovered, MENU_BEVEL);
            float is = 11.0f;
            textureBind(&s.touchGui);
            spriteDraw(&s.touchGui, (bx + (bs - is) / 2.0f) * UI_SCALE,
                       (by + (bs - is) / 2.0f) * UI_SCALE, is * UI_SCALE, is * UI_SCALE,
                       240.0f, 240.0f, is, is, WHITE);
            textW = bx - PANEL_X - 10.0f;
        }

        unsigned int nameCol = hovered ? 0xFFA0FFFFu : 0xFFFFFFFFu;
        fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 5.0f) * UI_SCALE,
                            sv.name, nameCol, UI_SCALE, textW);
        char addr[80];
        snprintf(addr, sizeof(addr), "%s:%d", sv.addr, sv.port);
        fontDrawTextClipped(&s.font, (PANEL_X + 5.0f) * UI_SCALE, (rY + 16.0f) * UI_SCALE,
                            addr, 0xFFBBBBBBu, UI_SCALE, textW);
    }
    sceGuScissor(0, 0, 480, 272);

    guiScrollbar((PANEL_X + PANEL_W - 2.0f) * UI_SCALE, pY * UI_SCALE, 2.0f * UI_SCALE,
                 pH * UI_SCALE, contentH * UI_SCALE, scroll * UI_SCALE);

    {
        float lb = 4.0f * MENU_PX + menuBarButtonW(s, kBarLabels[0]);
        float rb = barButtonX(s, 1);
        drawMenuHeader(s, "Play", 0.0f, VW, MENU_BAR_H, MENU_BAR_TEXT, lb, rb - lb);
    }
    for (int i = 0; i < 3; i++) {
        bool on = (s.joinUiRow == 0 && s.joinBarSel == i) || (i == 2 && s.joinEditMode);
        menuBarButton(s, barButtonX(s, i), menuBarButtonW(s, kBarLabels[i]), kBarLabels[i], on);
    }

    sceGuEnable(GU_DEPTH_TEST);
}

static JoinScreen s_joinScreen;
Screen& joinScreen() { return s_joinScreen; }
