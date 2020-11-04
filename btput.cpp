#define __cplusplus 201703L
#include <cctype>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

//using namespace std;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

const uint8_t DEFAULT_RFCOMM_CHANNEL {18};

struct options_t {
    uint8_t channel;
    const char *bdaddr;
    const char *pathname;
};

// https://www.gnu.org/software/libc/manual/html_node/Using-Getopt.html
static int parse_options(int argc, char *argv[], struct options_t *options)
{
    char *cvalue = NULL;
    int c;

    opterr = 0; // don't print error message to stderr

    while ((c = getopt(argc, argv, "c:")) != -1) {
        switch (c) {
        case 'c':
            cvalue = optarg;
            break;
        case '?':
            if (optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else if (std::isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        default:
            abort();
        }
    }

    const int num_mandatory_args = 2;
    if (optind + num_mandatory_args != argc) {
        cerr << "Usage: btput [OPTION] BDADDR FILENAME" << endl;
        cerr << "Send FILENAME to BDADDR." << endl;
        return 1;
    }

    // Set default options
    options->channel = DEFAULT_RFCOMM_CHANNEL;
    options->bdaddr = NULL;
    options->pathname = NULL;

    // Override default options with user-specified ones
    if (cvalue != NULL)
        options->channel = std::stoi(cvalue);
    options->bdaddr = argv[optind];
    options->pathname = argv[optind + 1];

    return 0;
}

// Print MAC address and name of remote device, and the RFCOMM channel we are using
static void print_remote(const bdaddr_t *bdaddr, uint8_t channel)
{
    const int dev_id = hci_get_route(NULL);
    const int dd = hci_open_dev(dev_id);
    char name[248] {};
    char addr[20] {};

    if (hci_read_remote_name(dd, bdaddr, sizeof(name), name, 0) < 0)
        strcpy(name, "[unknown]");
    ba2str(bdaddr, addr);
    printf("Connected to %s %s on channel %u\n", addr, name, channel);
    close(dd);
}

// Write N bytes of BUF to FD. Return 0 on success, or -1 on error.
static int write_bytes(int fd, const void *buf, ssize_t n)
{
    // Because the number of bytes written may be less than
    // the number of bytes we want to write, we need a loop
    // to write all of the bytes.
    for (ssize_t total_bytes_written {}; total_bytes_written < n;) {
        const ssize_t bytes_written = write(fd,
            (const uint8_t *) buf + total_bytes_written,
            n - total_bytes_written);
        if (bytes_written < 1)
            return -1;
        total_bytes_written += bytes_written;
    }

    return 0;
}

// Read header lines from CFD and return them. Newlines are discarded.
static vector<string> read_headers(int cfd)
{
    const int max_length {1024}; // guard against malformed input
    int count {};
    char current {}, previous {};
    vector<string> headers;
    string header;

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
static std::map<string, string> parse_headers(const vector<string>& headers)
{
    std::map<string, string> map;
    for (const auto& h : headers) {
        const std::size_t index {h.find_first_of(':')};
        if (index != string::npos) {
            const string k {h.substr(0, index)};
            const string v {h.substr(index + 1)};
            map[k] = v;
        }
    }
    return map;
}

// Read from FILENAME and write to SFD. Return 0 on success, or -1 on error.
static int put_file(std::string_view pathname, int sfd)
{
    // Open file and get file size
    const int fin = open(pathname.data(), O_RDONLY);
    if (fin == -1) {
        perror("open file");
        return -1;
    }
    struct stat st {};
    if (fstat(fin, &st) == -1 || st.st_size < 1 || !S_ISREG(st.st_mode)) {
        cerr << "not a regular file: " << pathname << endl;
        close(fin);
        return -1;
    }
    const ssize_t filesize {st.st_size};

    // Write request headers
    char headers[512] {};
    snprintf(headers, sizeof(headers), "method:PUT\npathname:%s\ncontent-length:%ld\n\n", pathname.data(), filesize);
    if (write_bytes(sfd, headers, strlen(headers)) != 0) {
        perror("\nwrite socket");
        close(fin);
        return -1;
    }

    // Read and parse response headers sent by server
    const auto res_headers {read_headers(sfd)};
    if (res_headers.size() == 0) {
        cerr << "read_headers error" << endl;
        close(fin);
        return -1;
    }
    const auto map {parse_headers(res_headers)};
    if (map.empty()) {
        cerr << "parse_headers error" << endl;
        close(fin);
        return -1;
    }
    for (const auto& [k, v] : map) {
        cout << "  " << k << ':' << v << endl;
    }

    // Read data from file and send to server
    ssize_t bytes_read;
    ssize_t bytes_done {};
    uint8_t buf[16 * 1024] {};
    while ((bytes_read = read(fin, buf, sizeof(buf))) > 0) {
        if (write_bytes(sfd, buf, bytes_read) != 0) {
            perror("\nwrite socket");
            close(fin);
            return -1;
        }
        bytes_done += bytes_read;
        cerr << '\r' << bytes_done << ' ' << bytes_done * 100 / filesize << '%';
    }
    cerr << endl;

    close(fin);
    return 0;
}

int main(int argc, char *argv[])
{
    struct options_t options{};
    if (parse_options(argc, argv, &options) != 0)
        return EXIT_FAILURE;

    // Allocate a socket
    const int sfd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // Set the connection parameters (who to connect to)
    struct sockaddr_rc rem_addr {};
    rem_addr.rc_family = AF_BLUETOOTH;
    rem_addr.rc_channel = options.channel;
    if (str2ba(options.bdaddr, &rem_addr.rc_bdaddr) != 0) {
        cerr << "invalid BDADDR" << endl;
        close(sfd);
        return EXIT_FAILURE;
    }

    // Connect to server
    if (connect(sfd, (struct sockaddr *) &rem_addr, sizeof(rem_addr)) == -1) {
        perror("connect failed");
        close(sfd);
        return EXIT_FAILURE;
    }
    print_remote(&rem_addr.rc_bdaddr, rem_addr.rc_channel);

    // Send file to remote device
    if (options.pathname != NULL) {
        put_file(options.pathname, sfd);
    }

    close(sfd);
    return EXIT_SUCCESS;
}
