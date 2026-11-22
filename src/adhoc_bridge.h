#pragma once
#include <pspsdk.h>
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
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
    uint8_t myMac[6] = {0};
    uint8_t peerMac[6] = {0};
    bool connected = false;
    bool hasPeer = false;
    float remoteX = 0.0f, remoteY = 64.0f, remoteZ = 0.0f;
    float remoteYaw = 0.0f, remotePitch = 0.0f;

    void init() {
        sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
        sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
        sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
        sceNetAdhocInit();

        struct productStruct product;
        std::strcpy(product.product, "ULUS99999");
        product.unknown = 0;
        sceNetAdhocctlInit(0x2000, 0x20, &product);
        sceNetAdhocctlConnect("MCPEPSP1");
        sceNetGetLocalEtherAddr(myMac);
    }

    void update() {
        if (!connected) {
            int state = 0;
            sceNetAdhocctlGetState(&state);
            if (state == 1) {
                pdpId = sceNetAdhocPdpCreate(myMac, 0x309, 0x4000, 0);
                connected = (pdpId >= 0);
            }
        }
    }

    void sendPos(float x, float y, float z, float yaw, float pitch) {
        if (!connected) return;
        NetPacket pkt{};
        pkt.type = 1;
        pkt.x = x; pkt.y = y; pkt.z = z;
        pkt.yaw = yaw; pkt.pitch = pitch;
        uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        sceNetAdhocPdpSend(pdpId, bcast, 0x309, &pkt, sizeof(NetPacket), 0, 1);
    }

    void sendBlock(int bx, int by, int bz, int blockId, int meta = 0) {
        if (!connected) return;
        NetPacket pkt{};
        pkt.type = 2;
        pkt.bx = bx; pkt.by = by; pkt.bz = bz;
        pkt.blockId = blockId; pkt.meta = meta;
        uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        sceNetAdhocPdpSend(pdpId, bcast, 0x309, &pkt, sizeof(NetPacket), 0, 1);
    }

    bool poll(NetPacket& out) {
        if (!connected) return false;
        uint16_t port = 0;
        int len = sizeof(NetPacket);
        int ret = sceNetAdhocPdpRecv(pdpId, peerMac, &port, &out, &len, 0, 1);
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
