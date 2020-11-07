#define __cplusplus 201703L
#include <unistd.h>
#include "common.h"

// Get string address and friendly name of remote bluetooth device
std::tuple<std::string, std::string> common::get_remote_bdname(const bdaddr_t *bdaddr)
{
    const int dev_id = hci_get_route(NULL);
    const int dd = hci_open_dev(dev_id);
    char addr[20] {};
    char name[248] {};

    ba2str(bdaddr, addr);
    if (hci_read_remote_name(dd, bdaddr, sizeof(name), name, 0) < 0)
        strcpy(name, "[unknown]");
    close(dd);
    return {addr, name};
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

// Read header lines from CFD and return them. Newlines are discarded.
std::vector<std::string> common::read_headers(int cfd)
{
    const int max_length {1024}; // guard against malformed input
    int count {};
    char current {}, previous {};
    std::vector<std::string> headers;
    std::string header;
    while (count < max_length && read(cfd, &current, 1) == 1) {
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
std::map<std::string, std::string> common::parse_headers(const std::vector<std::string>& headers)
{
    std::map<std::string, std::string> map;
    for (const std::string& h : headers) {
        const std::size_t index {h.find_first_of(':')};
        if (index != std::string::npos) {
            const std::string k {h.substr(0, index)};
            const std::string v {h.substr(index + 1)};
            map[k] = v;
        }
    }
    return map;
}
