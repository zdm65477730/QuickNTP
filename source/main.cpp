#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include <string>
#include <vector>

#include "ntp-client.hpp"
#include "servers.hpp"
#include "tesla-ext.hpp"

using namespace tsl;

TimeServiceType __nx_time_service_type = TimeServiceType_System;

time_t NTPClient::getTime() noexcept(false) {
    int server_sock, status;
    struct addrinfo hints, *servinfo, *ap;
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    ntp_packet packet = {.flags = NTP_FLAGS};

    hints = (struct addrinfo){.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};

    if ((status = getaddrinfo(m_server, m_port, &hints, &servinfo)) != 0) {
        throw NtpException(1, "UnableGetAddressInfoNtpGuiNtpExceptionText"_tr + std::string(gai_strerror(status)));
    }

    for (ap = servinfo; ap != NULL; ap = ap->ai_next) {
        server_sock = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
        if (server_sock != -1)
            break;
    }

    if (ap == NULL) {
        throw NtpException(2, "UnableCreateSocketNtpGuiNtpExceptionText"_tr);
    }

    struct timeval timeout = {.tv_sec = m_timeout, .tv_usec = 0};

    if (setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
        throw NtpException(3, "UnableSetRCVTimeoutNtpGuiNtpExceptionText"_tr);
    }

    if (setsockopt(server_sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
        throw NtpException(4, "UnableSetSNDTimeoutNtpGuiNtpExceptionText"_tr);
    }

    if ((status = sendto(server_sock, &packet, sizeof(packet), 0, ap->ai_addr, ap->ai_addrlen)) == -1) {
        throw NtpException(5, "UnableSendPacketNtpGuiNtpExceptionText"_tr);
    }

    if ((status = recvfrom(server_sock, &packet, sizeof(packet), 0, ap->ai_addr, &addrlen)) == -1) {
        if (errno == 11 || errno == 35) { // NX: 11, OTH: 35
            throw NtpException(6, "ConnectionTimeoutRetryNtpGuiNtpExceptionText"_tr + std::to_string(m_timeout) + "s");
        } else {
            throw NtpException(7, "UnableReceivePacketNtpGuiNtpExceptionText"_tr + std::to_string(errno));
        }
    }

    freeaddrinfo(servinfo);
    close(server_sock);

    packet.recv_ts_secs = ntohl(packet.recv_ts_secs);

    return packet.recv_ts_secs - UNIX_OFFSET;
}

class NtpGui : public tsl::Gui {
private:
    std::string Message = "";
    int currentServer = 0;
    bool blockFlag = false;
    std::vector<std::string> serverAddresses;
    std::vector<std::string> serverNames;

    std::string getCurrentServerAddress() {
        return serverAddresses[currentServer];
    }

    bool setNetworkSystemClock(time_t time) {
        Result rs = timeSetCurrentTime(TimeType_NetworkSystemClock, (uint64_t)time);
        if (R_FAILED(rs)) {
            return false;
        }
        return true;
    }

    void setTime() {
        std::string srv = getCurrentServerAddress();
        NTPClient* client = new NTPClient(srv.c_str());

        try {
            time_t ntpTime = client->getTime();

            if (setNetworkSystemClock(ntpTime)) {
                Message = "SyncedWithNtpGuiCustomDrawerUnscissoredText"_tr + srv;
            } else {
                Message = "UnableSetNetworkClockNtpGuiNtpExceptionText"_tr;
            }
        } catch (NtpException& e) {
            Message = "ErrorNtpGuiNtpExceptionText"_tr + e.what();
        }

        delete client;
    }

    void
    setNetworkTimeAsUser() {
        time_t userTime, netTime;

        Result rs = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&userTime);
        if (R_FAILED(rs)) {
            Message = "GetTimeUserNtpGuiCustomDrawerUnscissoredText"_tr + std::to_string(rs);
            return;
        }

        std::string usr = "UserTimeNtpGuiCustomDrawerUnscissoredText"_tr;
        std::string gr8 = "";
        rs = timeGetCurrentTime(TimeType_NetworkSystemClock, (u64*)&netTime);
        if (!R_FAILED(rs) && netTime < userTime) {
            gr8 = "GreatScottNtpGuiCustomDrawerUnscissoredText"_tr;
        }

        if (setNetworkSystemClock(userTime)) {
            Message = usr.append(gr8);
        } else {
            Message = "UnableSetNetworkClockNtpGuiNtpExceptionText"_tr;
        }
    }

    void getOffset() {
        time_t currentTime;
        Result rs = timeGetCurrentTime(TimeType_NetworkSystemClock, (u64*)&currentTime);
        if (R_FAILED(rs)) {
            Message = "GetTimeNetworkNtpGuiCustomDrawerUnscissoredText"_tr + std::to_string(rs);
            return;
        }

        std::string srv = getCurrentServerAddress();
        NTPClient* client = new NTPClient(srv.c_str());

        try {
            time_t ntpTimeOffset = client->getTimeOffset(currentTime);
            Message = "OffsetNtpGuiCustomDrawerUnscissoredText"_tr + std::to_string(ntpTimeOffset) + "s";
        } catch (NtpException& e) {
            Message = "ErrorNtpGuiNtpExceptionText"_tr + e.what();
        }

        delete client;
    }

    bool operationBlock(std::function<void()> fn) {
        if (!blockFlag) {
            blockFlag = true;
            Message = "";
            fn(); // TODO: Call async and set blockFlag to false
            blockFlag = false;
        }
        return !blockFlag;
    }

    std::function<std::function<bool(u64 keys)>(int key)> syncListener = [this](int key) {
        return [=, this](u64 keys) {
            if (keys & key) {
                return operationBlock([&]() {
                    setTime();
                });
            }
            return false;
        };
    };

    std::function<std::function<bool(u64 keys)>(int key)> offsetListener = [this](int key) {
        return [=, this](u64 keys) {
            if (keys & key) {
                return operationBlock([&]() {
                    getOffset();
                });
            }
            return false;
        };
    };

public:
    NtpGui() : serverAddresses(vectorPairValues(NTPSERVERS)),
               serverNames(vectorPairKeys(NTPSERVERS)) {}

    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::CustomOverlayFrame("PluginName"_tr, VERSION);

        auto list = new tsl::elm::List();

        list->setClickListener([this](u64 keys) {
            if (keys & (HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
                Message = "";
                return true;
            }
            return false;
        });

        list->addItem(new tsl::elm::CategoryHeader("PickServerOrSyncOrOffsetNtpGuiCategoryHeaderText"_tr));

        auto* trackbar = new tsl::elm::NamedStepTrackBarVector("\uE017", serverNames);
        trackbar->setValueChangedListener([this](u8 val) {
            currentServer = val;
            Message = "";
        });
        trackbar->setClickListener([this](u8 val) {
            return syncListener(HidNpadButton_A)(val) || offsetListener(HidNpadButton_Y)(val);
        });
        list->addItem(trackbar);

        auto* syncTimeItem = new tsl::elm::ListItem("SyncTimeNtpGuiListItemText"_tr);
        syncTimeItem->setClickListener(syncListener(HidNpadButton_A));
        list->addItem(syncTimeItem);

        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                          renderer->drawString("SyncTimeDescriptionNtpGuiCustomDrawerText"_tr.c_str(), false, x + 20, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
                      }),
                      50);

        auto* getOffsetItem = new tsl::elm::ListItem("GetOffsetNtpGuiListItemText"_tr);
        getOffsetItem->setClickListener(offsetListener(HidNpadButton_A));
        list->addItem(getOffsetItem);

        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                          renderer->drawString("GetOffsetDescriptionNtpGuiCustomDrawerText"_tr.c_str(), false, x + 20, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
                      }),
                      70);

        auto* setToInternalItem = new tsl::elm::ListItem("UserSetTimeNtpGuiListItemText"_tr);
        setToInternalItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                return operationBlock([&]() {
                    setNetworkTimeAsUser();
                });
            }
            return false;
        });
        list->addItem(setToInternalItem);

        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                          renderer->drawString("UserSetTimeDescriptionNtpGuiCustomDrawerText"_tr.c_str(), false, x + 20, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
                      }),
                      50);

        list->addItem(new tsl::elm::CustomDrawerUnscissored([&message = Message](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            if (!message.empty()) {
                renderer->drawString(message.c_str(), false, x + 5, tsl::cfg::FramebufferHeight - 100, 20, renderer->a(tsl::style::color::ColorText));
            }
        }));

        frame->setContent(list);
        return frame;
    }
};

class NtpOverlay : public tsl::Overlay {
public:
    virtual void initServices() override {
        std::string jsonStr = R"(
            {
                "PluginName": "QuickNTP",
                "UnableGetAddressInfoNtpGuiNtpExceptionText": "Unable to get address info: ",
                "UnableCreateSocketNtpGuiNtpExceptionText": "Unable to create the socket",
                "UnableSetRCVTimeoutNtpGuiNtpExceptionText": "Unable to set RCV timeout",
                "UnableSetSNDTimeoutNtpGuiNtpExceptionText": "Unable to set SND timeout",
                "UnableSendPacketNtpGuiNtpExceptionText": "Unable to send packet",
                "ConnectionTimeoutRetryNtpGuiNtpExceptionText": "Connection timeout, retry in ",
                "UnableReceivePacketNtpGuiNtpExceptionText": "Unable to receive packet: ",
                "UnableSetNetworkClockNtpGuiNtpExceptionText": "Unable to set network clock.",
                "ErrorNtpGuiNtpExceptionText": "Error: ",
                "SyncedWithNtpGuiCustomDrawerUnscissoredText": "Synced with ",
                "GetTimeUserNtpGuiCustomDrawerUnscissoredText": "GetTimeUser ",
                "UserTimeNtpGuiCustomDrawerUnscissoredText": "User time!",
                "GreatScottNtpGuiCustomDrawerUnscissoredText": " Great Scott!",
                "GetTimeNetworkNtpGuiCustomDrawerUnscissoredText": "GetTimeNetwork ",
                "OffsetNtpGuiCustomDrawerUnscissoredText": "Offset: ",
                "PickServerOrSyncOrOffsetNtpGuiCategoryHeaderText": "Pick server   |   \uE0E0  Sync   |   \uE0E3  Offset",
                "SyncTimeNtpGuiListItemText": "Sync time",
                "SyncTimeDescriptionNtpGuiCustomDrawerText": "Syncs the time with the selected server.",
                "GetOffsetNtpGuiListItemText": "Get offset",
                "GetOffsetDescriptionNtpGuiCustomDrawerText": "Gets the seconds offset with the selected server.\n\n\uE016  A value of ± 3 seconds is acceptable.",
                "UserSetTimeNtpGuiListItemText": "User-set time",
                "UserSetTimeDescriptionNtpGuiCustomDrawerText": "Sets the network time to the user-set time."
            }
        )";
        std::string lanPath = std::string("sdmc:/switch/.overlays/lang/") + APPTITLE + "/";
        fsdevMountSdmc();
        tsl::hlp::doWithSmSession([&lanPath, &jsonStr]{
            tsl::tr::InitTrans(lanPath, jsonStr);
        });
        fsdevUnmountDevice("sdmc");

        ASSERT_FATAL(socketInitializeDefault());
        ASSERT_FATAL(nifmInitialize(NifmServiceType_User));
        ASSERT_FATAL(timeInitialize());
        ASSERT_FATAL(smInitialize());
    }

    virtual void exitServices() override {
        socketExit();
        nifmExit();
        timeExit();
        smExit();
    }

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<NtpGui>();
    }
};

int main(int argc, char** argv) {
    return tsl::loop<NtpOverlay>(argc, argv);
}
