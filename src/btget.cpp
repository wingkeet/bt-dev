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
        const char *bdaddr;
        const char *pathname;
    };

    // https://www.gnu.org/software/libc/manual/html_node/Using-Getopt.html
    int parse_options(int argc, char *argv[], struct options_t *options)
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
            cerr << "Usage: btget [OPTION] BDADDR PATHNAME" << endl;
            cerr << "Get PATHNAME from BDADDR." << endl;
            return 1;
        }

        // Set default options
        options->channel = common::DEFAULT_RFCOMM_CHANNEL;
        options->bdaddr = NULL;
        options->pathname = NULL;

        // Override default options with user-specified ones
        if (cvalue != NULL)
            options->channel = std::stoi(cvalue);
        options->bdaddr = argv[optind];
        options->pathname = argv[optind + 1];

        return 0;
    }

    // Read from SFD and write to PATHNAME. Return 0 on success, or -1 on error.
    int get_file(int sfd, std::string_view pathname)
    {
        // Write request headers
        char headers[512] {};
        snprintf(headers, sizeof(headers), "method:GET\npathname:%s\n\n", pathname.data());
        if (common::write_bytes(sfd, headers, strlen(headers)) != 0) {
            perror("\nwrite socket");
            return -1;
        }

        // Read and parse response headers sent by server
        const auto& res_headers {common::read_headers(sfd)};
        if (res_headers.size() == 0) {
            cerr << "read_headers error" << endl;
            return -1;
        }
        const auto& map {common::parse_headers(res_headers)};
        if (map.empty()) {
            cerr << "parse_headers error" << endl;
            return -1;
        }
        for (const auto& [k, v] : map) {
            cout << "  " << k << ':' << v << endl;
        }

        // Check for 200 status code and get file size
        ssize_t filesize {};
        try {
            const int status_code {std::stoi(map.at("status"))};
            if (status_code != 200) {
                return -1;
            }
            filesize = std::stol(map.at("content-length"));
        }
        catch (const std::out_of_range& ex) {
            return -1;
        }

        // Open disk file for writing
        const std::filesystem::path dir {"transfer"}, file {pathname};
        std::ofstream fout {dir / file.filename(), std::ios::out | std::ios::binary};
        if (!fout.is_open()) {
            cerr << "open file failed" << endl;
            return -1;
        }

        // Read data from server and write to file
        ssize_t bytes_read;
        ssize_t bytes_done {};
        char buf[2 * 1024] {};
        while ((bytes_read = read(sfd, buf, sizeof(buf))) > 0) {
            fout.write(buf, bytes_read);
            bytes_done += bytes_read;
            cerr << '\r' << bytes_done << ' ' << bytes_done * 100 / filesize << '%';
        }
        cerr << endl;

        // Cleanup
        fout.close();
        return 0;
    }
} // unnamed namespace

int main(int argc, char *argv[])
{
    struct options_t options {};
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

    // Print address and name of server
    const auto& bdname {common::get_remote_bdname(&rem_addr.rc_bdaddr)};
    printf("Connected to %s %s on channel %u\n",
        options.bdaddr, bdname.c_str(), rem_addr.rc_channel);

    // Get file from server
    if (options.pathname != NULL) {
        get_file(sfd, options.pathname);
    }

    close(sfd);
    return EXIT_SUCCESS;
}
