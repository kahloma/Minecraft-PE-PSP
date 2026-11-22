#pragma once
#include <pspsdk.h>
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <array>
#include <cstring>
#include <cstdio>

struct NetPacket {
    uint8_t type; // 1 = PlayerPos, 2 = BlockSet
    float x, y, z, yaw, pitch;
    int bx, by, bz, blockId, meta;
};

class AdhocBridge {
public:
    int pdpId = -1;
    int lastState = -1;
    int lastErr = 0;
    std::array<uint8_t, 6> myMac{};
    std::array<uint8_t, 6> peerMac{};
    bool connected = false;
    bool hasPeer = false;
    bool applyingRemote = false;

    float remoteX = 0.0f, remoteY = 64.0f, remoteZ = 0.0f;
    float remoteYaw = 0.0f, remotePitch = 0.0f;

    void init() {
        int r1 = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
        int r2 = sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
        int r3 = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
        int r4 = sceNetAdhocInit();

        struct productStruct product;
        std::memset(&product, 0, sizeof(product));
        std::memcpy(product.product, "ULUS99999", 9);
        int r5 = sceNetAdhocctlInit(0x2000, 0x20, &product);
        lastErr = sceNetAdhocctlConnect("MCPEPSP1");
        if (lastErr == 0) lastErr = r1 ? r1 : (r2 ? r2 : (r3 ? r3 : (r4 ? r4 : r5)));
        sceNetGetLocalEtherAddr(myMac.data());
    }

    void update() {
        if (!connected) {
            sceNetAdhocctlGetState(&lastState);
            if (lastState == 1) {
                pdpId = sceNetAdhocPdpCreate(myMac.data(), 0x309, 0x4000, 0);
                connected = (pdpId >= 0);
                if (!connected) lastErr = pdpId;
            } else {
                static int retry = 0;
                if (++retry >= 120) {
                    retry = 0;
                    lastErr = sceNetAdhocctlConnect("MCPEPSP1");
                }
            }
        }
    }

    void sendPos(float x, float y, float z, float yaw, float pitch) {
        if (!connected) return;
        NetPacket pkt{};
        pkt.type = 1;
        pkt.x = x; pkt.y = y; pkt.z = z;
        pkt.yaw = yaw; pkt.pitch = pitch;
        std::array<uint8_t, 6> bcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
        sceNetAdhocPdpSend(pdpId, bcast.data(), 0x309, &pkt, sizeof(NetPacket), 0, 1);
    }

    void sendBlock(int bx, int by, int bz, int blockId, int meta = 0) {
        if (!connected) return;
        NetPacket pkt{};
        pkt.type = 2;
        pkt.bx = bx; pkt.by = by; pkt.bz = bz;
        pkt.blockId = blockId; pkt.meta = meta;
        std::array<uint8_t, 6> bcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
        sceNetAdhocPdpSend(pdpId, bcast.data(), 0x309, &pkt, sizeof(NetPacket), 0, 1);
    }

    bool poll(NetPacket& out) {
        if (!connected) return false;
        uint16_t port = 0;
        int len = sizeof(NetPacket);
        int ret = sceNetAdhocPdpRecv(pdpId, peerMac.data(), &port, &out, &len, 0, 1);
        if (ret == 0 && len == sizeof(NetPacket)) {
            hasPeer = true;
            if (out.type == 1) {
                remoteX = out.x; remoteY = out.y; remoteZ = out.z;
                remoteYaw = out.yaw; remotePitch = out.pitch;
            }
            return true;
        }
        return false;
    }
};

extern AdhocBridge g_adhoc;
