// One wire-level regression for reparent coordinates/events, cycle rejection,
// and DestroySubwindows preserving its parent and the next request boundary.
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int connection;
static uint16_t sequence;
static uint32_t child_id, parent_b;
static int check_events, reparents, unmaps, maps, cycle_rejected;
static uint16_t expected_error_sequence;
static void fail(const char *message) { fprintf(stderr, "%s errno=%d\n", message, errno); exit(1); }
static uint16_t r16(const unsigned char *b) { return b[0] | ((uint16_t)b[1] << 8); }
static uint32_t r32(const unsigned char *b) { return r16(b) | ((uint32_t)r16(b + 2) << 16); }
static void w16(unsigned char *b, uint16_t v) { b[0] = v; b[1] = v >> 8; }
static void w32(unsigned char *b, uint32_t v) { w16(b, v); w16(b + 2, v >> 16); }
static void read_all(void *buffer, size_t length) {
    size_t done = 0;
    while (done < length) {
        ssize_t count = read(connection, (char *)buffer + done, length - done);
        if (count <= 0) fail("X11 read failed or timed out");
        done += (size_t)count;
    }
}
static uint16_t send_request(unsigned char *bytes, size_t length) {
    w16(bytes + 2, (uint16_t)(length / 4));
    size_t done = 0;
    while (done < length) {
        ssize_t count = write(connection, bytes + done, length - done);
        if (count <= 0) fail("X11 write failed");
        done += (size_t)count;
    }
    return ++sequence;
}
static void reply(uint16_t wanted, unsigned char *result) {
    for (;;) {
        unsigned char packet[32]; read_all(packet, sizeof(packet));
        unsigned type = packet[0] & 0x7f;
        if (type == 0) {
            if (r16(packet + 2) == expected_error_sequence && packet[1] == 8) cycle_rejected++;
            else { fprintf(stderr, "unexpected_x11_error=%u sequence=%u opcode=%u\n", packet[1], r16(packet + 2), packet[10]); exit(1); }
        } else if (type == 1) {
            uint32_t remaining = r32(packet + 4) * 4;
            if (remaining > 65536) fail("oversized reply");
            unsigned char discard[1024];
            while (remaining) { size_t n = remaining < sizeof(discard) ? remaining : sizeof(discard); read_all(discard, n); remaining -= (uint32_t)n; }
            if (r16(packet + 2) == wanted) { if (result) memcpy(result, packet, 32); return; }
        } else if (check_events) {
            if (type == 21 && r32(packet + 8) == child_id) {
                if (r32(packet + 12) != parent_b || r16(packet + 16) != 37 || r16(packet + 18) != 49)
                    fail("bad ReparentNotify payload");
                reparents++;
            }
            if (type == 18 && r32(packet + 8) == child_id) unmaps++;
            if (type == 19 && r32(packet + 8) == child_id) maps++;
        }
    }
}
static void sync_requests(void) {
    unsigned char request[4] = {43};
    reply(send_request(request, sizeof(request)), NULL);
}
static void create_window(uint32_t id, uint32_t parent) {
    unsigned char request[40] = {1};
    w32(request + 4, id); w32(request + 8, parent);
    w16(request + 12, 5); w16(request + 14, 7);
    w16(request + 16, 80); w16(request + 18, 60); w16(request + 22, 1);
    w32(request + 28, (1u << 1) | (1u << 11));
    w32(request + 32, 0x203040); w32(request + 36, (1u << 17) | (1u << 19));
    send_request(request, sizeof(request));
    unsigned char map[8] = {8}; w32(map + 4, id); send_request(map, sizeof(map));
}
static uint16_t reparent(uint32_t window, uint32_t parent) {
    unsigned char request[16] = {7}; w32(request + 4, window); w32(request + 8, parent);
    w16(request + 12, 37); w16(request + 14, 49);
    return send_request(request, sizeof(request));
}
int main(int argc, char **argv) {
    if (argc != 2) fail("provide the application's private X11 socket");
    connection = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connection < 0) fail("socket");
    struct timeval timeout = {3,0}; setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    struct sockaddr_un address = {0}; address.sun_family = AF_UNIX;
    if (strlen(argv[1]) >= sizeof(address.sun_path)) fail("socket path too long");
    strcpy(address.sun_path, argv[1]);
    if (connect(connection, (struct sockaddr *)&address, sizeof(address)) != 0) fail("connect");
    unsigned char setup[12] = {'l',0,11};
    if (write(connection, setup, sizeof(setup)) != sizeof(setup)) fail("setup write");
    unsigned char header[8]; read_all(header, 8);
    if (header[0] != 1) fail("setup rejected");
    size_t length = r16(header + 6) * 4;
    unsigned char *body = malloc(length);
    if (!body || length < 32 || length > 262144) fail("invalid setup length");
    read_all(body, length);
    uint32_t base = r32(body + 4);
    size_t screen = 32 + ((r16(body + 16) + 3) & ~(size_t)3) + body[21] * 8;
    if (screen + 40 > length) fail("missing root screen");
    uint32_t root = r32(body + screen), parent_a = base | 1;
    parent_b = base | 2; child_id = base | 3; free(body);
    create_window(parent_a, root); create_window(parent_b, root); create_window(child_id, parent_a);
    sync_requests();
    check_events = 1;
    reparent(child_id, parent_b); sync_requests();
    if (reparents != 3 || unmaps != 2 || maps != 2) fail("missing reparent/unmap/map events");
    printf("reparent_events=%d unmap_events=%d map_events=%d\n", reparents, unmaps, maps);
    check_events = 0;
    unsigned char geometry[8] = {14}, packet[32]; w32(geometry + 4, child_id);
    reply(send_request(geometry, sizeof(geometry)), packet);
    if (r16(packet + 12) != 37 || r16(packet + 14) != 49) fail("wrong relative geometry");
    unsigned char tree[8] = {15}; w32(tree + 4, child_id);
    reply(send_request(tree, sizeof(tree)), packet);
    if (r32(packet + 12) != parent_b) fail("wrong parent");
    expected_error_sequence = reparent(parent_b, child_id); sync_requests();
    if (cycle_rejected != 1) fail("tree cycle not rejected");
    unsigned char destroy[8] = {5}; w32(destroy + 4, parent_b); send_request(destroy, sizeof(destroy));
    w32(tree + 4, parent_b); reply(send_request(tree, sizeof(tree)), packet);
    if (r16(packet + 16) != 0) fail("children not destroyed");
    sync_requests();
    puts("relative_geometry=1 parent_verified=1 cycle_rejected=1 destroy_children_only=1 request_stream_intact=1 complete=1");
    close(connection); return 0;
}
