#include <arpa/inet.h>
#include <errno.h>
#include <exception>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define UNIX_OFFSET 2208988800L

#define NTP_DEFAULT_PORT "123"
#define DEFAULT_TIMEOUT 3

// Flags 00|100|011 for li=0, vn=4, mode=3
#define NTP_FLAGS 0x23

typedef struct {
    uint8_t flags;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint8_t referenceID[4];
    uint32_t ref_ts_secs;
    uint32_t ref_ts_frac;
    uint32_t origin_ts_secs;
    uint32_t origin_ts_frac;
    uint32_t recv_ts_secs;
    uint32_t recv_ts_fracs;
    uint32_t transmit_ts_secs;
    uint32_t transmit_ts_frac;

} ntp_packet;

struct NtpException : public std::exception {
protected:
    int m_code;
    std::string m_message;

public:
    NtpException(int code, std::string message) {
        m_code = code;
        m_message = message;
    }

    std::string what() noexcept {
        return m_message;
    }
};

class NTPClient {
private:
    int m_timeout;
    const char* m_port;
    const char* m_server;

public:
    NTPClient(const char* server, const char* port, int timeout) {
        m_server = server;
        m_port = port;
        m_timeout = timeout;
    }

    NTPClient(const char* server, const char* port) : NTPClient(server, port, DEFAULT_TIMEOUT) {}
    NTPClient(const char* server) : NTPClient(server, NTP_DEFAULT_PORT, DEFAULT_TIMEOUT) {}
    NTPClient() : NTPClient("pool.ntp.org", NTP_DEFAULT_PORT, DEFAULT_TIMEOUT) {}

    void setTimeout(int timeout) {
        m_timeout = timeout;
    }

    time_t getTime() noexcept(false);

    long getTimeOffset(time_t currentTime) noexcept(false) {
        time_t ntpTime = getTime();
        return currentTime - ntpTime;
    }
};
