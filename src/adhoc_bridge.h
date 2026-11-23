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
    uint8_t type; // 1 = PlayerPos, 2 = BlockSet, 3 = WorldInfo, 4 = ItemDrop, 5 = MobSync
    uint8_t senderMac[6];
    float x, y, z, yaw, pitch;
    float vx, vy, vz;
    float walkPos, walkSpeed, atkAnim;
    uint8_t sneaking;
    short heldId;
    unsigned char heldData;
    int heldCount;
    int bx, by, bz, blockId, meta;
    long seed;
    int gameMode, worldType;
    int entId, entType;
    uint8_t entRemoved;
    char nick[24];
    char skin[64];
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
    bool isJoiner = false;

    bool hasHostWorld = false;
    long hostSeed = 0;
    int hostMode = 1;
    int hostType = 0;

    char myNick[24] = "Player";
    char peerNick[24] = "Peer";
    char peerSkin[64] = "";

    float prevX = 0.0f, prevY = 64.0f, prevZ = 0.0f;
    float remoteX = 0.0f, remoteY = 64.0f, remoteZ = 0.0f;
    float remoteYaw = 0.0f, remotePitch = 0.0f;
    float remoteWalkPos = 0.0f, remoteWalkSpeed = 0.0f, remoteWalkSpeedO = 0.0f;
    float remoteAtk = 0.0f, remoteAtkO = 0.0f;
    bool remoteSneaking = false;
    short remoteHeldId = 0;
    unsigned char remoteHeldData = 0;
    float lerpT = 1.0f;

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

        char sysNick[64] = {0};
        if (sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, sysNick, sizeof(sysNick)) == 0 && sysNick[0]) {
            std::strncpy(myNick, sysNick, sizeof(myNick) - 1);
            myNick[sizeof(myNick) - 1] = '\0';
        }
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

    void sendRaw(NetPacket& pkt, bool broadcast = false) {
        if (!connected) return;
        std::memcpy(pkt.senderMac, myMac.data(), 6);
        std::array<uint8_t, 6> bcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
        uint8_t* dst = (!broadcast && hasPeer) ? peerMac.data() : bcast.data();
        sceNetAdhocPdpSend(pdpId, dst, 0x309, &pkt, sizeof(NetPacket), 0, 1);
    }

    void sendWorldInfo(long seed, int gameMode, int worldType, const char* skinStr) {
        NetPacket pkt{};
        pkt.type = 3;
        pkt.seed = seed;
        pkt.gameMode = gameMode;
        pkt.worldType = worldType;
        std::strncpy(pkt.nick, myNick, sizeof(pkt.nick) - 1);
        if (skinStr) std::strncpy(pkt.skin, skinStr, sizeof(pkt.skin) - 1);
        sendRaw(pkt, true);
    }

    void sendPlayer(float x, float y, float z, float yaw, float pitch,
                    float walkPos, float walkSpeed, float atkAnim,
                    bool sneaking, short heldId, unsigned char heldData) {
        NetPacket pkt{};
        pkt.type = 1;
        pkt.x = x; pkt.y = y; pkt.z = z;
        pkt.yaw = yaw; pkt.pitch = pitch;
        pkt.walkPos = walkPos; pkt.walkSpeed = walkSpeed; pkt.atkAnim = atkAnim;
        pkt.sneaking = sneaking ? 1 : 0;
        pkt.heldId = heldId; pkt.heldData = heldData;
        sendRaw(pkt, false);
    }

    void sendBlock(int bx, int by, int bz, int blockId, int meta = 0) {
        NetPacket pkt{};
        pkt.type = 2;
        pkt.bx = bx; pkt.by = by; pkt.bz = bz;
        pkt.blockId = blockId; pkt.meta = meta;
        sendRaw(pkt, false);
    }

    void sendItemDrop(float x, float y, float z, float vx, float vy, float vz,
                      short id, unsigned char data, int count) {
        NetPacket pkt{};
        pkt.type = 4;
        pkt.x = x; pkt.y = y; pkt.z = z;
        pkt.vx = vx; pkt.vy = vy; pkt.vz = vz;
        pkt.heldId = id; pkt.heldData = data; pkt.heldCount = count;
        sendRaw(pkt, false);
    }

    void sendMobSync(int entId, int entType, float x, float y, float z,
                     float yaw, float pitch, bool removed) {
        NetPacket pkt{};
        pkt.type = 5;
        pkt.entId = entId; pkt.entType = entType;
        pkt.x = x; pkt.y = y; pkt.z = z;
        pkt.yaw = yaw; pkt.pitch = pitch;
        pkt.entRemoved = removed ? 1 : 0;
        sendRaw(pkt, false);
    }

    bool poll(NetPacket& out) {
        if (!connected) return false;
        uint16_t port = 0;
        int len = sizeof(NetPacket);
        std::array<uint8_t, 6> srcMac{};
        int ret = sceNetAdhocPdpRecv(pdpId, srcMac.data(), &port, &out, &len, 0, 1);
        if (ret == 0 && len == sizeof(NetPacket)) {
            if (std::memcmp(out.senderMac, myMac.data(), 6) == 0) return false;
            peerMac = srcMac;
            if (out.type == 1) {
                if (!hasPeer) {
                    prevX = out.x; prevY = out.y; prevZ = out.z;
                } else {
                    prevX = remoteX; prevY = remoteY; prevZ = remoteZ;
                }
                remoteX = out.x; remoteY = out.y; remoteZ = out.z;
                remoteYaw = out.yaw; remotePitch = out.pitch;
                remoteWalkSpeedO = remoteWalkSpeed;
                remoteWalkPos = out.walkPos;
                remoteWalkSpeed = out.walkSpeed;
                remoteAtkO = remoteAtk;
                remoteAtk = out.atkAnim;
                remoteSneaking = (out.sneaking != 0);
                remoteHeldId = out.heldId;
                remoteHeldData = out.heldData;
                lerpT = 0.0f;
            } else if (out.type == 3) {
                hasHostWorld = true;
                hostSeed = out.seed;
                hostMode = out.gameMode;
                hostType = out.worldType;
                if (out.nick[0]) std::strncpy(peerNick, out.nick, sizeof(peerNick) - 1);
                if (out.skin[0]) std::strncpy(peerSkin, out.skin, sizeof(peerSkin) - 1);
            }
            hasPeer = true;
            return true;
        }
        return false;
    }
};

extern AdhocBridge g_adhoc;
