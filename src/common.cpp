#define __cplusplus 201703L
#include <unistd.h>
#include "common.h"

// Get friendly name of remote bluetooth device
std::string common::get_remote_bdname(const bdaddr_t *bdaddr)
{
    const int dev_id = hci_get_route(NULL);
    const int dd = hci_open_dev(dev_id);
    char bdname[248] {};
    if (hci_read_remote_name(dd, bdaddr, sizeof(bdname), bdname, 0) < 0)
        strcpy(bdname, "[unknown]");
    close(dd);
    return bdname;
}

// Write N bytes of BUF to FD. Return 0 on success, or -1 on error.
int common::write_bytes(int fd, const void *buf, ssize_t n)
{
    // Because the number of bytes actually written may be less
    // than the number of bytes we want to write, we need a loop
    // to write all of the bytes.
    for (ssize_t total {}, actual {}; total < n; total += actual) {
        actual = write(fd, (const uint8_t *) buf + total, n - total);
        if (actual < 1)
            return -1;
    }
    return 0;
}

// Read header lines from FD and return them. Newlines are discarded.
std::vector<std::string> common::read_headers(int fd)
{
    const int max_length {1024}; // safeguard against malformed input
    int count {};
    char current {}, previous {};
    std::vector<std::string> headers;
    std::string header;
    while (count < max_length && read(fd, &current, 1) == 1) {
        count++;
        if (current != '\n') {
            header += current;
        }
        else {
            if (previous != '\n') {
                headers.push_back(header);
                header.clear();
            }
            else {
                return headers;
            }
        }
        previous = current;
    }
    return {}; // return empty vector on error
}

// Parse HEADERS and return map of key-value pairs.
std::map<std::string, std::string>
common::parse_headers(const std::vector<std::string>& headers)
{
    std::map<std::string, std::string> map;
    for (const auto& h : headers) {
        const auto& index {h.find_first_of(':')};
        if (index != std::string::npos) {
            const auto& k {h.substr(0, index)};
            const auto& v {h.substr(index + 1)};
            map[k] = v;
        }
    }
    return map;
}
