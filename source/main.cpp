#ifndef VERSION
#define VERSION "v1.0.0"
#endif

#define TESLA_INIT_IMPL
#include <minIni.h>
#include <tesla.hpp>

#include <string>
#include <vector>

#include "ntp-client.hpp"
#include "tesla-ext.hpp"

using namespace tsl;

TimeServiceType __nx_time_service_type = TimeServiceType_System;

const char* iniLocations[] = {
    "/config/" APPTITLE "/config.ini",
};
const char* iniSection = "Servers";

const char* defaultServerAddress = "pool.ntp.org";
const char* defaultServerName = "NTP Pool Main";

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
        } catch (const NtpException& e) {
            Message = "ErrorNtpGuiNtpExceptionText"_tr + std::string(e.what());
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
        } catch (const NtpException& e) {
            Message = "ErrorNtpGuiNtpExceptionText"_tr + std::string(e.what());
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
    NtpGui() {
        char key[INI_BUFFERSIZE];
        char value[INI_BUFFERSIZE];

        const char* iniFile = iniLocations[0];
        for (const char* loc : iniLocations) {
            INI_FILETYPE fp;
            if (ini_openread(loc, &fp)) {
                iniFile = loc;
                ini_close(&fp);
            }
        }

        int idx = 0;
        while (ini_getkey(iniSection, idx++, key, INI_BUFFERSIZE, iniFile) > 0) {
            ini_gets(iniSection, key, "", value, INI_BUFFERSIZE, iniFile);
            serverAddresses.push_back(value);

            std::string keyStr = key;
            std::replace(keyStr.begin(), keyStr.end(), '_', ' ');
            serverNames.push_back(keyStr);
        }

        if (serverNames.empty() || serverAddresses.empty()) {
            serverNames.push_back(defaultServerName);
            serverAddresses.push_back(defaultServerAddress);
        }
    }

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
                "UnableSetReceiveTimeoutNtpGuiNtpExceptionText": "Unable to set socket receive timeout",
                "UnableSetSendTimeoutNtpGuiNtpExceptionText": "Unable to set socket send timeout",
                "UnableConnectToNTPServerNtpGuiNtpExceptionText": "Unable to connect to NTP server",
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
        ASSERT_FATAL(smInitialize()); // Needed
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
