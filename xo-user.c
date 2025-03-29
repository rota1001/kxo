#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "game.h"

#define XO_STATUS_FILE "/sys/module/kxo/initstate"
#define XO_DEVICE_FILE "/dev/kxo"
#define XO_DEVICE_ATTR_FILE "/sys/class/kxo/kxo/kxo_state"

#define IOCTL_READ_SIZE 0
#define IOCTL_READ_LIST 1

static bool status_check(void)
{
    FILE *fp = fopen(XO_STATUS_FILE, "r");
    if (!fp) {
        printf("kxo status : not loaded\n");
        return false;
    }

    char read_buf[20];
    fgets(read_buf, 20, fp);
    read_buf[strcspn(read_buf, "\n")] = 0;
    if (strcmp("live", read_buf)) {
        printf("kxo status : %s\n", read_buf);
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

static struct termios orig_termios;

static void raw_mode_disable(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void raw_mode_enable(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(raw_mode_disable);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~IXON;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static bool read_attr, end_attr;

static void listen_keyboard_handler(void)
{
    int attr_fd = open(XO_DEVICE_ATTR_FILE, O_RDWR);
    char input;

    if (read(STDIN_FILENO, &input, 1) == 1) {
        char buf;
        switch (input) {
        case 16: /* Ctrl-P */
            read(attr_fd, &buf, 1);
            buf ^= 1;
            read_attr ^= 1;
            write(attr_fd, &buf, 1);
            if (!read_attr)
                printf("Stopping to display the chess board...\n");
            break;
        case 17: /* Ctrl-Q */
            read(attr_fd, &buf, 1);
            buf |= 4;
            read_attr = false;
            end_attr = true;
            write(attr_fd, &buf, 1);
            printf("Stopping the kernel space tic-tac-toe game...\n");
            break;
        }
    }
    close(attr_fd);
}

static int draw_board(const unsigned int table)
{
    int k = 0;
    puts("\n");
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < (BOARD_SIZE << 1) - 1 && k < N_GRIDS; j++) {
            unsigned int bit = table >> (k << 1);
            if (j & 1)
                putchar('|');
            else {
                putchar(!(bit & 2) ? ' ' : (bit & 1 ? 'O' : 'X'));
                k++;
            }
        }

        putchar('\n');

        for (int j = 0; j < (BOARD_SIZE << 1) - 1; j++)
            putchar('-');

        putchar('\n');
    }
    return 0;
}

void print_record(uint64_t record)
{
    uint64_t record_size = record;
    record_size ^= (record_size >> 4);
    record_size ^= (record_size >> 8);
    record_size ^= (record_size >> 16);
    record_size ^= (record_size >> 32);
    record_size &= 0xf;
    if (!record_size)
        record_size = 16;
    record_size <<= 2;
    printf("Moves: ");
    for (int i = 0; i < record_size; i += 4) {
        unsigned int move = (record >> i) & 15;
        printf("%c%u", 'A' + (move >> 2), move & 3);
        if (i != record_size - 4)
            printf(" -> ");
    }
    puts("");
}

void show_record(int device_fd)
{
    int size = ioctl(device_fd, IOCTL_READ_SIZE, 0);
    if (size < 0) {
        puts("Read board record fail");
        return;
    }
    for (int i = 0; i < size; i++) {
        uint64_t record;
        if (ioctl(device_fd, (((unsigned) i) << 1) | IOCTL_READ_LIST,
                  &record)) {
            puts("Read board record fail");
            return;
        }
        print_record(record);
    }
}

int main(int argc, char *argv[])
{
    if (!status_check())
        exit(1);

    raw_mode_enable();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    unsigned int display_buf;

    fd_set readset;
    int device_fd = open(XO_DEVICE_FILE, O_RDONLY);
    int max_fd = device_fd > STDIN_FILENO ? device_fd : STDIN_FILENO;
    read_attr = true;
    end_attr = false;

    while (!end_attr) {
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        FD_SET(device_fd, &readset);

        int result = select(max_fd + 1, &readset, NULL, NULL, NULL);
        if (result < 0) {
            printf("Error with select system call\n");
            exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &readset)) {
            FD_CLR(STDIN_FILENO, &readset);
            listen_keyboard_handler();
        } else if (read_attr && FD_ISSET(device_fd, &readset)) {
            FD_CLR(device_fd, &readset);
            printf("\033[H\033[J"); /* ASCII escape code to clear the screen */
            read(device_fd, &display_buf, sizeof(display_buf));
            draw_board(display_buf);
        }
    }

    raw_mode_disable();

    show_record(device_fd);

    fcntl(STDIN_FILENO, F_SETFL, flags);

    close(device_fd);

    return 0;
}
