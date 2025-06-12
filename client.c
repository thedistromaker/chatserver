#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "config.h"

typedef struct {
    char ip[64];
    char name[MAX_NAME_LENGTH];
    int sockfd;
    WINDOW *win;
    char messages[MAX_MSGS][MAX_MSG_LENGTH];
    int msg_count;
} ChatTab;

ChatTab tabs[MAX_TABS];
int current_tab = 0;
WINDOW *input_win;

void draw_tab_bar() {
    move(0, 0);
    for (int i = 0; i < MAX_TABS; i++) {
        if (i == current_tab)
            attron(A_REVERSE);
        if (tabs[i].name[0])
            printw(" %s ", tabs[i].name);
        else
            printw(" Tab %d ", i + 1);
        if (i == current_tab)
            attroff(A_REVERSE);
        printw(" ");
    }
    clrtoeol();
    refresh();
}

void draw_messages() {
    ChatTab *tab = &tabs[current_tab];
    werase(tab->win);
    box(tab->win, 0, 0);
    int max_lines = LINES - 5;
    int start = (tab->msg_count > max_lines) ? tab->msg_count - max_lines : 0;
    for (int i = start, y = 1; i < tab->msg_count; i++, y++) {
        mvwprintw(tab->win, y, 2, "%s", tab->messages[i]);
    }
    wrefresh(tab->win);
}

void switch_tab(int direction) {
    current_tab = (current_tab + direction + MAX_TABS) % MAX_TABS;
    draw_tab_bar();
    draw_messages();
    wrefresh(input_win);
}

int connect_to_server(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(ip)
    };

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void *receiver_thread(void *arg) {
    ChatTab *tab = (ChatTab *)arg;
    char buffer[MAX_MSG_LENGTH];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t len = recv(tab->sockfd, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) break;
        if (tab->msg_count < MAX_MSGS) {
            snprintf(tab->messages[tab->msg_count++], MAX_MSG_LENGTH, "Server: %s", buffer);
            if (tab == &tabs[current_tab]) draw_messages();
        }
    }
    if (tab->msg_count < MAX_MSGS)
        snprintf(tab->messages[tab->msg_count++], MAX_MSG_LENGTH, "Disconnected.");
    if (tab == &tabs[current_tab]) draw_messages();
    close(tab->sockfd);
    tab->sockfd = -1;
    return NULL;
}

void add_message(ChatTab *tab, const char *fmt, ...) {
    if (tab->msg_count >= MAX_MSGS) {
        memmove(tab->messages, tab->messages + 1, sizeof(tab->messages[0]) * (MAX_MSGS - 1));
        tab->msg_count--;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(tab->messages[tab->msg_count++], MAX_MSG_LENGTH, fmt, args);
    va_end(args);
}

int main() {
    for (int i = 0; i < MAX_TABS; i++) {
        tabs[i].sockfd = -1;
        tabs[i].msg_count = 0;
        tabs[i].name[0] = 0;
        tabs[i].ip[0] = 0;
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    draw_tab_bar();

    for (int i = 0; i < MAX_TABS; i++) {
        tabs[i].win = newwin(LINES - 4, COLS, 1, 0);
        box(tabs[i].win, 0, 0);
        wrefresh(tabs[i].win);
    }

    input_win = newwin(3, COLS, LINES - 3, 0);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 1, "> ");
    wrefresh(input_win);

    draw_messages();

    char input[MAX_MSG_LENGTH];
    pthread_t recv_threads[MAX_TABS];

    while (1) {
        werase(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 1, "> ");
        wmove(input_win, 1, 3);  // move cursor after prompt
        wrefresh(input_win);

        echo();  // Enable echo to show user input
        wgetnstr(input_win, input, sizeof(input) - 1);
        noecho(); // Disable again after reading

        if (strcmp(input, "/quit") == 0) break;
        else if (strcmp(input, "/tab") == 0) {
            switch_tab(1);
        }
        else if (strncmp(input, "/join ", 6) == 0) {
            char ip[64], name[MAX_NAME_LENGTH];
            if (sscanf(input + 6, "%63s %63s", ip, name) == 2) {
                int sockfd = connect_to_server(ip, PORT);
                if (sockfd < 0) {
                    add_message(&tabs[current_tab], "Failed to connect to %s", ip);
                } else {
                    if (tabs[current_tab].sockfd >= 0) close(tabs[current_tab].sockfd);
                    strncpy(tabs[current_tab].ip, ip, sizeof(tabs[current_tab].ip));
                    strncpy(tabs[current_tab].name, name, sizeof(tabs[current_tab].name));
                    tabs[current_tab].sockfd = sockfd;
                    add_message(&tabs[current_tab], "Connected to %s (%s)", name, ip);
                    pthread_create(&recv_threads[current_tab], NULL, receiver_thread, &tabs[current_tab]);
                    pthread_detach(recv_threads[current_tab]);
                }
            } else {
                add_message(&tabs[current_tab], "Usage: /join <ip> <name>");
            }
            draw_tab_bar();
            draw_messages();
        }
        else if (strncmp(input, "/fav ", 5) == 0) {
            char ip[64], name[MAX_NAME_LENGTH];
            if (sscanf(input + 5, "%63s %63s", ip, name) == 2) {
                FILE *f = fopen("favorites.txt", "a");
                if (f) {
                    fprintf(f, "%s %s\n", ip, name);
                    fclose(f);
                    add_message(&tabs[current_tab], "Saved favorite: %s (%s)", name, ip);
                } else {
                    add_message(&tabs[current_tab], "Failed to open favorites.txt");
                }
            } else {
                add_message(&tabs[current_tab], "Usage: /fav <ip> <name>");
            }
            draw_messages();
        }
        else if (strcmp(input, "/list") == 0) {
            FILE *f = fopen("favorites.txt", "r");
            if (f) {
                add_message(&tabs[current_tab], "Favorites:");
                char ip[64], name[MAX_NAME_LENGTH];
                while (fscanf(f, "%63s %63s", ip, name) == 2) {
                    add_message(&tabs[current_tab], " - %s (%s)", name, ip);
                }
                fclose(f);
            } else {
                add_message(&tabs[current_tab], "No favorites found.");
            }
            draw_messages();
        }
        else if (tabs[current_tab].sockfd >= 0) {
            send(tabs[current_tab].sockfd, input, strlen(input), 0);
            add_message(&tabs[current_tab], "You: %s", input);
            draw_messages();
        } else {
            add_message(&tabs[current_tab], "Not connected to any server. Use /join <ip> <name>");
            draw_messages();
        }
    }

    for (int i = 0; i < MAX_TABS; i++) {
        if (tabs[i].sockfd >= 0) close(tabs[i].sockfd);
    }

    endwin();
    return 0;
}
