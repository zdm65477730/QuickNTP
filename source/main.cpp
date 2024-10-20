#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include <string>
#include <vector>

#include "ntp-client.hpp"
#include "servers.hpp"
#include "tesla-ext.hpp"

TimeServiceType __nx_time_service_type = TimeServiceType_System;

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
                Message = "Synced with " + srv;
            } else {
                Message = "Unable to set network clock.";
            }
        } catch (NtpException& e) {
            Message = "Error: " + e.what();
        }

        delete client;
    }

    void
    setNetworkTimeAsUser() {
        time_t userTime, netTime;

        Result rs = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&userTime);
        if (R_FAILED(rs)) {
            Message = "GetTimeUser " + std::to_string(rs);
            return;
        }

        std::string usr = "User time!";
        std::string gr8 = "";
        rs = timeGetCurrentTime(TimeType_NetworkSystemClock, (u64*)&netTime);
        if (!R_FAILED(rs) && netTime < userTime) {
            gr8 = " Great Scott!";
        }

        if (setNetworkSystemClock(userTime)) {
            Message = usr.append(gr8);
        } else {
            Message = "Unable to set network clock.";
        }
    }

    void getOffset() {
        time_t currentTime;
        Result rs = timeGetCurrentTime(TimeType_NetworkSystemClock, (u64*)&currentTime);
        if (R_FAILED(rs)) {
            Message = "GetTimeNetwork " + std::to_string(rs);
            return;
        }

        std::string srv = getCurrentServerAddress();
        NTPClient* client = new NTPClient(srv.c_str());

        try {
            time_t ntpTimeOffset = client->getTimeOffset(currentTime);
            Message = "Offset: " + std::to_string(ntpTimeOffset) + "s";
        } catch (NtpException& e) {
            Message = "Error: " + e.what();
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
        auto frame = new tsl::elm::CustomOverlayFrame("QuickNTP", "by NedEX - v1.2.8-1");

        auto list = new tsl::elm::List();

        list->setClickListener([this](u64 keys) {
            if (keys & (HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
                Message = "";
                return true;
            }
            return false;
        });

        list->addItem(new tsl::elm::CategoryHeader("Pick server   |   \uE0E0  Sync   |   \uE0E3  Offset"));

        auto* trackbar = new tsl::elm::NamedStepTrackBarVector("\uE017", serverNames);
        trackbar->setValueChangedListener([this](u8 val) {
            currentServer = val;
            Message = "";
        });
        trackbar->setClickListener([this](u8 val) {
            return syncListener(HidNpadButton_A)(val) || offsetListener(HidNpadButton_Y)(val);
        });
        list->addItem(trackbar);

        auto* syncTimeItem = new tsl::elm::ListItem("Sync time");
        syncTimeItem->setClickListener(syncListener(HidNpadButton_A));
        list->addItem(syncTimeItem);

        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                          renderer->drawString("Syncs the time with the selected server.", false, x + 20, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
                      }),
                      50);

        auto* getOffsetItem = new tsl::elm::ListItem("Get offset");
        getOffsetItem->setClickListener(offsetListener(HidNpadButton_A));
        list->addItem(getOffsetItem);

        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                          renderer->drawString("Gets the seconds offset with the selected server.\n\n\uE016  A value of ± 3 seconds is acceptable.", false, x + 20, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
                      }),
                      70);

        auto* setToInternalItem = new tsl::elm::ListItem("User-set time");
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
                          renderer->drawString("Sets the network time to the user-set time.", false, x + 20, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
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
