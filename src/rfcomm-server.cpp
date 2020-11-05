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
};

// Parse command line arguments into OPTIONS. Return 0 on success, or -1 on error.
static int parse_options(int argc, char *argv[], struct options_t *options)
{
    char *cvalue = NULL;
    int c;

    opterr = 0; // don't print error message to stderr

    // https://www.gnu.org/software/libc/manual/html_node/Using-Getopt.html
    // https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
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
            return -1;
        default:
            abort();
        }
    }

    // Set default options
    options->channel = DEFAULT_RFCOMM_CHANNEL;

    // Override default options with user-specified ones
    if (cvalue != NULL)
        options->channel = std::stoi(cvalue);

    return 0;
}

// Print MAC address and name of remote bluetooth device.
static void print_remote(const bdaddr_t *bdaddr)
{
    const int dev_id = hci_get_route(NULL);
    const int dd = hci_open_dev(dev_id);
    char name[248] {};
    char addr[20] {};

    if (hci_read_remote_name(dd, bdaddr, sizeof(name), name, 0) < 0)
        strcpy(name, "[unknown]");
    ba2str(bdaddr, addr);
    printf("Accepted connection from %s %s\n", addr, name);
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

// Write response headers to CFD. Return 0 on success, or -1 on error.
static int write_res_headers(int cfd, int status_code, ssize_t filesize = 0)
{
    char headers[128] {};

    if (filesize > 0)
        snprintf(headers, sizeof(headers), "status:%d\ncontent-length:%ld\n\n", status_code, filesize);
    else
        snprintf(headers, sizeof(headers), "status:%d\n\n", status_code);

    if (write_bytes(cfd, headers, strlen(headers)) != 0) {
        perror("\nwrite socket");
        return -1;
    }
    return 0;
}

// Read data from CFD and write to PATHNAME. Return 0 on success, or -1 on error.
static int put_file(int cfd, std::string_view pathname, size_t filesize)
{
    // Open disk file for writing
    std::ofstream ofs {pathname.data(), std::ios::out | std::ios::binary};
    if (!ofs.is_open()) {
        cerr << "open file failed" << endl;
        write_res_headers(cfd, 500);
        return -1;
    }

    // Write response headers
    if (write_res_headers(cfd, 200) == -1) {
        ofs.close();
        return -1;
    }

    // Read data from client and write to file
    ssize_t bytes_read;
    ssize_t bytes_done {};
    char buf[2 * 1024] {};
    while ((bytes_read = read(cfd, buf, sizeof(buf))) > 0) {
        ofs.write(buf, bytes_read);
        bytes_done += bytes_read;
        cerr << '\r' << bytes_done << ' ' << bytes_done * 100 / filesize << '%';
    }
    cerr << endl;

    // Cleanup
    ofs.close();
    return 0;
}

// Read data from PATHNAME and write to CFD. Return 0 on success, or -1 on error.
static int get_file(int cfd, std::string_view pathname)
{
    // Open file and get file size
    const int fin = open(pathname.data(), O_RDONLY);
    if (fin == -1) {
        perror("open file");
        write_res_headers(cfd, 404);
        return -1;
    }
    struct stat st {};
    if (fstat(fin, &st) == -1 || st.st_size < 1 || !S_ISREG(st.st_mode)) {
        cerr << "not a regular file: " << pathname << endl;
        close(fin);
        write_res_headers(cfd, 404);
        return -1;
    }
    const ssize_t filesize {st.st_size};

    // Write response headers
    if (write_res_headers(cfd, 200, filesize) == -1) {
        close(fin);
        return -1;
    }

    // Read data from file and write to client
    ssize_t bytes_read;
    ssize_t bytes_done {};
    uint8_t buf[16 * 1024] {};
    while ((bytes_read = read(fin, buf, sizeof(buf))) > 0) {
        if (write_bytes(cfd, buf, bytes_read) != 0) {
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

static int wait_client(int sfd)
{
    // Wait for a client to connect
    cout << "Waiting for connection..." << endl;
    sockaddr_rc rem_addr {};
    socklen_t opt {sizeof(rem_addr)};
    const int cfd = accept(sfd, (sockaddr *) &rem_addr, &opt);
    print_remote(&rem_addr.rc_bdaddr);

    // Read and parse headers sent by client
    const auto headers {read_headers(cfd)};
    if (headers.size() == 0) {
        cerr << "read_headers error" << endl;
        close(cfd);
        return -1;
    }
    const auto map {parse_headers(headers)};
    if (map.empty()) {
        cerr << "parse_headers error" << endl;
        close(cfd);
        return -1;
    }
    for (const auto& [k, v] : map) {
        cout << "  " << k << ':' << v << endl;
    }

    int status {-1};
    try {
        // Call either put_file() or get_file(), depending on the "method" header
        const std::string_view method {map.at("method")};
        if (method == "PUT") {
            const std::filesystem::path p {map.at("pathname")};
            const std::filesystem::path dir {"transfer"};
            const std::filesystem::path pathname {dir / p.filename()};
            const size_t filesize {std::stoul(map.at("content-length"))};
            status = put_file(cfd, pathname.string(), filesize);
        }
        else if (method == "GET") {
            const string pathname {map.at("pathname")};
            status = get_file(cfd, pathname);
        }
        else {
            cerr << "invalid method: " << method << endl;
        }
    }
    catch (const std::out_of_range& ex) {
        cerr << "out of range exception: " << ex.what() << endl;
    }

    // Cleanup
    close(cfd);
    return status;
}

int main(int argc, char *argv[])
{
    struct options_t options {};
    if (parse_options(argc, argv, &options) != 0)
        return EXIT_FAILURE;

    // Allocate a socket
    const int sfd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (sfd == -1) {
        perror("create socket");
        return EXIT_FAILURE;
    }

    // Bind socket to the first available local bluetooth adapter
    const bdaddr_t BDADDR_ANY_INITIALIZER {};
    struct sockaddr_rc loc_addr {};
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = BDADDR_ANY_INITIALIZER;
    loc_addr.rc_channel = options.channel;
    if (bind(sfd, (struct sockaddr *) &loc_addr, sizeof(loc_addr)) == -1) {
        perror("bind socket");
        return EXIT_FAILURE;
    }

    // Put socket into listening mode
    if (listen(sfd, 1) == -1) {
        perror("socket listen");
        return EXIT_FAILURE;
    }
    cout << "Listening on channel " << +loc_addr.rc_channel << endl;

    while (true) {
        wait_client(sfd);
    }

    close(sfd);
    return EXIT_SUCCESS;
}
