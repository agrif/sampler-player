#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/timeb.h>

#include <microhttpd.h>

#include "linux/ioctls.h"

#define STRBUFSIZE 512

/*
 * First off, some generic sampler/player drivers
 */

/* swaps bit order in a single byte */
static uint8_t swaptable[256] = {
     0, 128,  64, 192,  32, 160,  96, 224,  16, 144,  80, 208,  48,
    176, 112, 240,   8, 136,  72, 200,  40, 168, 104, 232,  24, 152,
     88, 216,  56, 184, 120, 248,   4, 132,  68, 196,  36, 164, 100,
    228,  20, 148,  84, 212,  52, 180, 116, 244,  12, 140,  76, 204,
     44, 172, 108, 236,  28, 156,  92, 220,  60, 188, 124, 252,   2,
    130,  66, 194,  34, 162,  98, 226,  18, 146,  82, 210,  50, 178,
    114, 242,  10, 138,  74, 202,  42, 170, 106, 234,  26, 154,  90,
    218,  58, 186, 122, 250,   6, 134,  70, 198,  38, 166, 102, 230,
     22, 150,  86, 214,  54, 182, 118, 246,  14, 142,  78, 206,  46,
    174, 110, 238,  30, 158,  94, 222,  62, 190, 126, 254,   1, 129,
     65, 193,  33, 161,  97, 225,  17, 145,  81, 209,  49, 177, 113,
    241,   9, 137,  73, 201,  41, 169, 105, 233,  25, 153,  89, 217,
     57, 185, 121, 249,   5, 133,  69, 197,  37, 165, 101, 229,  21,
    149,  85, 213,  53, 181, 117, 245,  13, 141,  77, 205,  45, 173,
    109, 237,  29, 157,  93, 221,  61, 189, 125, 253,   3, 131,  67,
    195,  35, 163,  99, 227,  19, 147,  83, 211,  51, 179, 115, 243,
     11, 139,  75, 203,  43, 171, 107, 235,  27, 155,  91, 219,  59,
    187, 123, 251,   7, 135,  71, 199,  39, 167, 103, 231,  23, 151,
     87, 215,  55, 183, 119, 247,  15, 143,  79, 207,  47, 175, 111,
    239,  31, 159,  95, 223,  63, 191, 127, 255
};

typedef struct {
    char* name;
    enum {
        SP_SAMPLER,
        SP_PLAYER,
    } type;
    int fd;

    int sample_width;
    int sample_bits;
    int sample_length;
    int time_bits;
    int time_length;
    int bits;
    int length;

    uint8_t* data;
} SPDevice;

int sp_device_sysfs_read(SPDevice* self, const char* key, char* buffer, size_t buffer_len) {
    char fname[STRBUFSIZE];
    int i;
    FILE* fp;
    snprintf(fname, STRBUFSIZE, "/sys/block/%s/device/%s", self->name, key);
    fp = fopen(fname, "r");
    if (!fp)
        return 0;
    i = fread(buffer, 1, buffer_len, fp);
    buffer[i] = 0;
    fclose(fp);

    // strip off whitespace at end
    for (i = i - 1; i >= 0; i--) {
        if (buffer[i] == '\n')
            buffer[i] = 0;
        else
            break;
    }
    return 1;
}

int sp_device_sysfs_read_int(SPDevice* self, const char* key) {
    char buffer[STRBUFSIZE];
    if (!sp_device_sysfs_read(self, key, buffer, STRBUFSIZE)) {
        return -1;
    }
    return atoi(buffer);
}

int sp_device_get_enabled(SPDevice* self) {
    return ioctl(self->fd, OSUQL_SP_GET_ENABLED);
}

void sp_device_set_enabled(SPDevice* self, int enabled) {
    ioctl(self->fd, OSUQL_SP_SET_ENABLED, enabled);
}

int sp_device_get_done(SPDevice* self) {
    return ioctl(self->fd, OSUQL_SP_GET_DONE);
}

static inline void sp_device_swap_data(SPDevice* self) {
    unsigned int i;
    for (i = 0; i < self->length; i++)
        self->data[i] = swaptable[self->data[i]];
}

const uint8_t* sp_device_read(SPDevice* self) {
    uint8_t* data = self->data;
    unsigned int length = self->length;
    lseek(self->fd, 0, SEEK_SET);
    while (length) {
        ssize_t amount = read(self->fd, data, length);
        if (amount <= 0)
            return NULL;
        data += amount;
        if (amount > length)
            length = 0;
        else
            length -= amount;
    }
    sp_device_swap_data(self);
    return self->data;
}

int sp_device_write(SPDevice* self) {
    uint8_t* data = self->data;
    unsigned int length = self->length;
    lseek(self->fd, 0, SEEK_SET);
    while (length) {
        ssize_t amount = write(self->fd, data, length);
        if (amount <= 0)
            return 0;
        data += amount;
        if (amount > length)
            length = 0;
        else
            length -= amount;
    }
    sp_device_swap_data(self);
    return 1;
}

void sp_device_close(SPDevice* self) {
    if (self) {
        free(self->name);
        if (self->fd >= 0)
            close(self->fd);
        if (self->data)
            free(self->data);
    }
}

SPDevice* sp_device_open(const char* name) {
    char buffer[STRBUFSIZE];
    
    SPDevice* self = calloc(1, sizeof(SPDevice));
    self->fd = -1;

    self->name = strdup(name);

    if (!sp_device_sysfs_read(self, "type", buffer, STRBUFSIZE)) {
        sp_device_close(self);
        return NULL;
    }
    if (strcmp(buffer, "sampler") == 0) {
        self->type = SP_SAMPLER;
    } else if (strcmp(buffer, "player") == 0) {
        self->type = SP_PLAYER;
    } else {
        sp_device_close(self);
        return NULL;
    }

    self->sample_width = sp_device_sysfs_read_int(self, "sample_width");
    if (self->sample_width < 0) {
        sp_device_close(self);
        return NULL;
    }

    self->sample_bits = sp_device_sysfs_read_int(self, "sample_bits");
    if (self->sample_bits < 0) {
        sp_device_close(self);
        return NULL;
    }

    self->sample_length = sp_device_sysfs_read_int(self, "sample_length");
    if (self->sample_length < 0) {
        sp_device_close(self);
        return NULL;
    }

    self->time_bits = sp_device_sysfs_read_int(self, "time_bits");
    if (self->time_bits < 0) {
        sp_device_close(self);
        return NULL;
    }

    self->time_length = sp_device_sysfs_read_int(self, "time_length");
    if (self->time_length < 0) {
        sp_device_close(self);
        return NULL;
    }

    self->bits = sp_device_sysfs_read_int(self, "bits");
    if (self->bits < 0) {
        sp_device_close(self);
        return NULL;
    }

    self->length = sp_device_sysfs_read_int(self, "length");
    if (self->length < 0) {
        sp_device_close(self);
        return NULL;
    }
    
    snprintf(buffer, STRBUFSIZE, "/dev/%s", name);
    if (self->type == SP_SAMPLER) {
        self->fd = open(buffer, O_RDONLY | O_DIRECT | O_SYNC);
    } else {
        self->fd = open(buffer, O_WRONLY | O_DIRECT | O_SYNC);
    }
    if (self->fd < 0) {
        sp_device_close(self);
        return NULL;
    }

    posix_memalign((void**)&(self->data), 512, self->length);

    return self;
}

SPDevice* sp_sampler_open(const char* name) {
    SPDevice* dev = sp_device_open(name);
    if (dev && dev->type != SP_SAMPLER) {
        sp_device_close(dev);
        return NULL;
    }
    return dev;
}

SPDevice* sp_player_open(const char* name) {
    SPDevice* dev = sp_device_open(name);
    if (dev && dev->type != SP_PLAYER) {
        sp_device_close(dev);
        return NULL;
    }
    return dev;
}

typedef struct {
    SPDevice* samp;
    SPDevice* play;

    uint8_t* inputs;
    unsigned int inputs_length;
    const uint8_t* outputs;
    unsigned int outputs_length;
} SPPair;

const uint8_t* sp_pair_run(SPPair* self) {
    sp_device_set_enabled(self->samp, 0);
    sp_device_set_enabled(self->play, 0);

    sp_device_write(self->play);

    sp_device_set_enabled(self->samp, 1);
    sp_device_set_enabled(self->play, 1);

    while (!sp_device_get_done(self->samp));
    while (!sp_device_get_done(self->play));

    sp_device_set_enabled(self->samp, 0);
    sp_device_set_enabled(self->play, 0);

    return sp_device_read(self->samp);
}

void sp_pair_close(SPPair* self) {
    if (self) {
        if (self->samp)
            sp_device_close(self->samp);
        if (self->play)
            sp_device_close(self->play);
    }
}

SPPair* sp_pair_open(const char* sampname, const char* playname) {
    SPPair* self = calloc(1, sizeof(SPPair));
    self->samp = sp_sampler_open(sampname);
    self->play = sp_player_open(playname);

    if (!self->samp || !self->play) {
        sp_pair_close(self);
        return NULL;
    }

    self->inputs = self->play->data;
    self->inputs_length = self->play->length;
    self->outputs = self->samp->data;
    self->outputs_length = self->samp->length;

    return self;
}

/*
 * now, some infrastructure for dealing with HTTP requests
 */

typedef struct _SPState SPState;

typedef int (*SPRequestFunc)(SPState*, SPPair*, struct MHD_Connection*, const uint8_t*, size_t*);

struct _SPState {
    SPRequestFunc handler;
    int have_arrsize;
    int incorrect_data;
    uint32_t arrsize1;
    uint32_t arrsize2;
    uint32_t i;
};

#define QUEUE_RESPONSE(conn, code, resp) do {           \
        int ret = MHD_NO;                               \
        if (resp) {                                     \
            ret = MHD_queue_response(conn, code, resp); \
            MHD_destroy_response(resp);                 \
        }                                               \
        return ret;                                     \
    } while(0)

#define QUEUE_STATIC_RESPONSE(conn, code, typ, str) do {            \
        struct MHD_Response* response = MHD_create_response_from_buffer(strlen(str), (void*)(str), MHD_RESPMEM_PERSISTENT); \
        if (response) {                                                 \
            MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, typ);     \
        }                                                               \
        QUEUE_RESPONSE(conn, code, response);                      \
    } while(0)

#define QUEUE_ERROR_RESPONSE(conn, code, str) QUEUE_STATIC_RESPONSE(conn, code, "text/html", "<html><body><h1>" str "</h1></body></html>\n")

/*
 * our HTTP request handlers
 */

static int handler_run(SPState* state, SPPair* pair, struct MHD_Connection* conn, const uint8_t* upload_data, size_t* data_size) {
    if (*data_size) {
        uint32_t bytesize;
        /* FIXME failure here does not result in proper http responses */
        if (!state->have_arrsize) {
            if (*data_size < 8) {
                *data_size = 0;
                state->incorrect_data = 1;
                return MHD_YES;
            }
            
            state->arrsize1 = (upload_data[0] << 24) | (upload_data[1] << 16) | (upload_data[2] << 8) | upload_data[3];
            state->arrsize2 = (upload_data[4] << 24) | (upload_data[5] << 16) | (upload_data[6] << 8) | upload_data[7];

            if (state->arrsize1 > pair->play->time_length || state->arrsize2 > pair->play->sample_width) {
                *data_size = 0;
                state->incorrect_data = 1;
                return MHD_YES;
            }
            
            *data_size -= 8;
            upload_data += 8;
            state->have_arrsize = 1;
            state->i = 0;
        }

        bytesize = (state->arrsize2 + 7) / 8;

        while (state->i < pair->inputs_length && *data_size) {
            /* if we're in a no-data zone, add zeroes until we're out */
            while (state->i % pair->play->sample_length >= bytesize && state->i < pair->inputs_length) {
                pair->inputs[state->i] = 0;
                state->i++;
            }

            pair->inputs[state->i] = *upload_data;

            upload_data += 1;
            *data_size -= 1;
            state->i++;
        }

        return MHD_YES;
    } else {
        struct MHD_Response* response;

        if (state->incorrect_data) {
            QUEUE_ERROR_RESPONSE(conn, MHD_HTTP_BAD_REQUEST, "Bad Request");
        }

        /* fill in the rest with zeroes */
        while (state->i < pair->inputs_length) {
            pair->inputs[state->i] = 0;
            state->i++;
        }

        if (sp_pair_run(pair)) {
            uint8_t* outputs = malloc(pair->outputs_length + 8);

            outputs[0] = (pair->samp->time_length >> 24) & 0xff;
            outputs[1] = (pair->samp->time_length >> 16) & 0xff;
            outputs[2] = (pair->samp->time_length >>  8) & 0xff;
            outputs[3] = (pair->samp->time_length >>  0) & 0xff;

            outputs[4] = (pair->samp->sample_width >> 24) & 0xff;
            outputs[5] = (pair->samp->sample_width >> 16) & 0xff;
            outputs[6] = (pair->samp->sample_width >>  8) & 0xff;
            outputs[7] = (pair->samp->sample_width >>  0) & 0xff;

            /* technically we should exclude the extra 0's, but I'm ok
             * with this for now
             */
            memcpy(outputs + 8, pair->outputs, pair->outputs_length);
            
            response = MHD_create_response_from_buffer(pair->outputs_length + 8, outputs, MHD_RESPMEM_MUST_FREE);
            if (response) {
                MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/octet-stream");
            }
            QUEUE_RESPONSE(conn, MHD_HTTP_OK, response);
        } else {
            QUEUE_ERROR_RESPONSE(conn, MHD_HTTP_INTERNAL_SERVER_ERROR, "Internal Server Error");
        }        
    }
}

static int handler_default(void* cls, struct MHD_Connection* conn, const char* url, const char* method, const char* verison, const char* upload_data, size_t* upload_data_size, void** ptr) {
    SPPair* pair = cls;
    SPState* state = *ptr;

    if (!(*ptr)) {
        /* this is the first call, it has only read headers */
        state = *ptr = calloc(1, sizeof(SPState));
        if (!(*ptr))
            QUEUE_ERROR_RESPONSE(conn, MHD_HTTP_INTERNAL_SERVER_ERROR, "Internal Server Error");

        if (strcmp(url, "/run") == 0 && strcmp(method, "POST") == 0) {
            state->handler = handler_run;
            return MHD_YES;
        } else {
            QUEUE_ERROR_RESPONSE(conn, MHD_HTTP_NOT_FOUND, "Not Found");
        }
    }

    return state->handler(state, pair, conn, upload_data, upload_data_size);
}

static void request_completed(void* cls, struct MHD_Connection* conn, void** ptr, enum MHD_RequestTerminationCode toe) {
    if (*ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/*
 * tying it all together
 */

int main(int argc, char** argv) {
    struct MHD_Daemon* d;
    SPPair* pair;
    int i;
    struct timeb start, end;
    float seconds;
    if (argc != 2) {
        fprintf(stderr, "%s PORT\n", argv[0]);
        return 1;
    }

    pair = sp_pair_open("sampler0", "player0");
    if (!pair) {
        fprintf(stderr, "failed to open sampler/player\n");
        return 1;
    }
    
    d = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, atoi(argv[1]), NULL, NULL, &handler_default, pair, MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL, MHD_OPTION_END);

    if (!d) {
        sp_pair_close(pair);
        return 1;
    }

#define NUM_ITERS 100
    //ftime(&start);
    //for (i = 0; i < NUM_ITERS; i++) {
    //    sp_pair_run(pair);
    //}
    //ftime(&end);
    //seconds = 1.0 * (end.time - start.time) + 0.001 * (end.millitm - start.millitm);
    //printf("%i iters in %f seconds: %f / second\n", NUM_ITERS, seconds, NUM_ITERS / seconds);

    (void) getc(stdin);
    MHD_stop_daemon(d);
    sp_pair_close(pair);
    return 0;
}
