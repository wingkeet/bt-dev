// common.h

#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <string>
#include <tuple>
#include <vector>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

namespace common
{
    inline const uint8_t DEFAULT_RFCOMM_CHANNEL {22};

    std::tuple<std::string, std::string> get_remote_bdname(const bdaddr_t *bdaddr);
    int write_bytes(int fd, const void *buf, ssize_t n);
    std::vector<std::string> read_headers(int fd);
    std::map<std::string, std::string> parse_headers(const std::vector<std::string>& headers);
}

#endif // COMMON_H
