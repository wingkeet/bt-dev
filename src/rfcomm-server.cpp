#define __cplusplus 201703L
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"

namespace {
    // Everything inside this unnamed namespace has internal linkage.
    // The practice of using 'static' is de facto deprecated.

    using std::cout;
    using std::cerr;
    using std::endl;
    using std::string;
    using std::vector;

    struct options_t {
        uint8_t channel;
    };

    // Parse command line arguments into OPTIONS. Return 0 on success, or -1 on error.
    int parse_options(int argc, char *argv[], struct options_t *options)
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
        options->channel = common::DEFAULT_RFCOMM_CHANNEL;

        // Override default options with user-specified ones
        if (cvalue != NULL)
            options->channel = std::stoi(cvalue);

        return 0;
    }

    // Write response headers to CFD. Return 0 on success, or -1 on error.
    int write_res_headers(int cfd, int status_code, ssize_t filesize = 0)
    {
        char headers[128] {};

        if (filesize > 0) {
            snprintf(headers, sizeof(headers), "status:%d\ncontent-length:%ld\n\n",
                status_code, filesize);
        }
        else {
            snprintf(headers, sizeof(headers), "status:%d\n\n", status_code);
        }

        if (common::write_bytes(cfd, headers, strlen(headers)) != 0) {
            perror("\nwrite socket");
            return -1;
        }
        return 0;
    }

    // Read data from CFD and write to PATHNAME. Return 0 on success, or -1 on error.
    int put_file(int cfd, std::string_view pathname, size_t filesize)
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
    int get_file(int cfd, std::string_view pathname)
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
            if (common::write_bytes(cfd, buf, bytes_read) != 0) {
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

    int wait_client(int sfd)
    {
        // Wait for a client to connect
        cout << "Waiting for connection..." << endl;
        sockaddr_rc rem_addr {};
        socklen_t opt {sizeof(rem_addr)};
        const int cfd = accept(sfd, (sockaddr *) &rem_addr, &opt);

        // Print address and name of remote bluetooth device
        char bdaddr[18] {};
        ba2str(&rem_addr.rc_bdaddr, bdaddr);
        const auto& bdname {common::get_remote_bdname(&rem_addr.rc_bdaddr)};
        printf("Accepted connection from %s %s\n", bdaddr, bdname.c_str());

        // Read and parse headers sent by client
        const auto& headers {common::read_headers(cfd)};
        if (headers.size() == 0) {
            cerr << "read_headers error" << endl;
            close(cfd);
            return -1;
        }
        const auto& map {common::parse_headers(headers)};
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
                namespace fs = std::filesystem;
                const fs::path p {map.at("pathname")};
                const fs::path dir {"transfer"};
                const fs::path pathname {dir / p.filename()};
                const size_t filesize {std::stoul(map.at("content-length"))};
                status = put_file(cfd, pathname.string(), filesize);
            }
            else if (method == "GET") {
                const std::string_view pathname {map.at("pathname")};
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
} // unnamed namespace

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
