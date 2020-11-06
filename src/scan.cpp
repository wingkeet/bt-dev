/**
 * This program scans nearby Bluetooth devices and displays results
 * in this format: <bdaddr> <device-class> <friendly-name>
 * Example: 11:22:33:44:55:AB 0x5A020C My Phone
 * The target device must first be set to discoverable mode.
 * Device classes are defined in:
 * https://www.bluetooth.com/specifications/assigned-numbers/baseband/
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

int main(int argc, char *argv[])
{
    const int dev_id = hci_get_route(NULL);
    const int sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0) {
        perror("opening socket");
        return EXIT_FAILURE;
    }

    const int len {8}; // inquiry lasts for at most 1.28 * len seconds
    const int max_rsp {255};
    inquiry_info *ii {NULL};
    const long flags {IREQ_CACHE_FLUSH};

    // hci_inquiry() is equivalent to 'hcitool inq'
    const int num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
    if (num_rsp < 0) {
        perror("hci_inquiry");
        close(sock);
        return EXIT_FAILURE;
    }

    for (int i {}; i < num_rsp; i++) {
        char addr[20] {};
        char name[248] {};

        ba2str(&ii[i].bdaddr, addr);
        // hci_read_remote_name() is equivalent to 'hcitool name <bdaddr>'
        if (hci_read_remote_name(sock, &ii[i].bdaddr, sizeof(name), name, 0) < 0)
            strcpy(name, "[unknown]");
        printf("%s 0x%02X%02X%02X %s\n", addr, ii[i].dev_class[2],
            ii[i].dev_class[1], ii[i].dev_class[0], name);
    }

    free(ii);
    close(sock);
    return EXIT_SUCCESS;
}
