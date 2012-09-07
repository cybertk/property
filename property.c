/**
 * \copyright 2012 Kyan <kyan.ql.he@gmail.com>
 *
 * Kyan He <kyan.ql.he@gmail.com> @ Wed Sep  5 14:13:18 CST 2012
 *
 * Inspired by Android system_properties.
 */

#define ERROR printf

#ifdef ANDROID
#include <sys/atomics.h>
#else
#include <time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>

#include <fcntl.h>

#include "config.h"
#include "property.h"

int futex_wait(volatile void *ftx, int val, const struct timespec *timeout)
{
#ifdef ANDROID
    return __futex_wait(ftx, val, timeout);
#else
    nanosleep(timeout, NULL);
    return 0;
#endif
}

int futex_wake(volatile void *ftx, int count)
{
#ifdef ANDROID
    return __futex_wake(volatile void *ftx, int count);
#else
    return 0;
#endif
}

typedef struct prop_area prop_area;
typedef struct prop_info prop_info;

#define TOC_NAME_LEN(toc)       ((toc) >> 24)
#define TOC_TO_INFO(area, toc)  \
    ((prop_info*) (((char*) area) + ((toc) & 0xFFFFFF)))

struct prop_area {
    unsigned volatile count;
    unsigned volatile serial;
    unsigned reserved[6];
    unsigned toc[1];
};

#define SERIAL_VALUE_LEN(serial) ((serial) >> 24)
#define SERIAL_DIRTY(serial) ((serial) & 1)

struct prop_info {
    char name[PROP_NAME_MAX];
    unsigned volatile serial;
    char value[PROP_VALUE_MAX];
};

static int persistent_properties_loaded = 0;

static unsigned dummy_props = 0;
static prop_area *__system_property_area__ = (void*) &dummy_props;
static prop_info *pa_info_array;


// 8 header words + 247 toc words = 1020 bytes.
// 1024 bytes header and toc + 247 prop_infos @ 128 bytes = 32768 bytes.
#define PA_COUNT_MAX  247
#define PA_INFO_START 1024
#define PA_SIZE       32768

int __system_properties_init(void)
{
    prop_area *pa;

    pa = malloc(PA_SIZE);
    if (!pa) return -1;

    memset(pa, 0, PA_SIZE);

    pa_info_array = (void*) (((char*) pa) + PA_INFO_START);
    __system_property_area__ = pa;

    return 0;
}

const prop_info *__system_property_find(const char *name)
{
    prop_area *pa = __system_property_area__;
    unsigned count = pa->count;
    unsigned *toc = pa->toc;
    unsigned len = strlen(name);
    prop_info *pi;

    while(count--) {
        unsigned entry = *toc++;
        if(TOC_NAME_LEN(entry) != len) continue;

        pi = TOC_TO_INFO(pa, entry);
        if(memcmp(name, pi->name, len)) continue;

        return pi;
    }

    return 0;
}

/**
 * Look up a property by name, copying its value and a
 * \0 terminator to the provided pointer.  The total bytes
 * copied will be no greater than PROP_VALUE_MAX.
 *
 * \return  The string length of the value. A property that
 * is not * defined is identical to a property with a 
 * length 0 value.
 */
int __system_property_read(const prop_info *pi, char *name, char *value)
{
    unsigned serial, len;

    for(;;) {
        serial = pi->serial;
        while(SERIAL_DIRTY(serial)) {
            struct timespec ts;

            ts.tv_sec = 0;
            ts.tv_nsec = 100;

            futex_wait((volatile void *)&pi->serial, serial, 0);
            serial = pi->serial;
        }
        len = SERIAL_VALUE_LEN(serial);
        memcpy(value, pi->value, len + 1);
        if(serial == pi->serial) {
            if(name != 0) {
                strcpy(name, pi->name);
            }
            return len;
        }
    }
}

const prop_info *__system_property_find_nth(unsigned n)
{
    prop_area *pa = __system_property_area__;

    if(n >= pa->count) {
        return 0;
    } else {
        return TOC_TO_INFO(pa, pa->toc[n]);
    }
}

int __system_property_get(const char *name, char *value)
{
    const prop_info *pi = __system_property_find(name);

    if(pi != 0) {
        return __system_property_read(pi, 0, value);
    } else {
        value[0] = 0;
        return 0;
    }
}

static void load_properties(char *data)
{
    char *key, *value, *eol, *sol, *tmp;

    sol = data;
    while((eol = strchr(sol, '\n'))) {
        key = sol;
        *eol++ = 0;
        sol = eol;

        value = strchr(key, '=');
        if(value == 0) continue;
        *value++ = 0;

        // Strip spaces before key and ignore comment line.
        while(isspace(*key)) key++;
        if(*key == '#') continue;

        // Strip spaces after key.
        tmp = value - 2;
        while((tmp > key) && isspace(*tmp)) *tmp-- = 0;

        // Strip spaces around value.
        while(isspace(*value)) value++;
        tmp = eol - 2;
        while((tmp > value) && isspace(*tmp)) *tmp-- = 0;

        property_set(key, value);
    }
}

static void load_properties_from_file(const char *fn)
{
    char *data;
    unsigned sz;

    data = (char *)load_file(fn, &sz);

    if (data) {
        load_properties(data);
        free(data);
    }
}

static void update_prop_info(prop_info *pi, const char *value, unsigned len)
{
    pi->serial = pi->serial | 1;
    memcpy(pi->value, value, len + 1);
    pi->serial = (len << 24) | ((pi->serial + 1) & 0xffffff);
    futex_wake(&pi->serial, INT32_MAX);
}

static void load_persistent_properties()
{
    DIR* dir = opendir(PERSISTENT_PROPERTY_DIR);
    struct dirent*  entry;
    char path[PATH_MAX];
    char value[PROP_VALUE_MAX];
    int fd, length;

    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp("persist.", entry->d_name, strlen("persist.")))
                continue;
#if HAVE_DIRENT_D_TYPE
            if (entry->d_type != DT_REG)
                continue;
#endif
            /* open the file and read the property value */
            snprintf(path, sizeof(path), "%s/%s", PERSISTENT_PROPERTY_DIR, entry->d_name);
            fd = open(path, O_RDONLY);
            if (fd >= 0) {
                length = read(fd, value, sizeof(value) - 1);
                if (length >= 0) {
                    value[length] = 0;
                    property_set(entry->d_name, value);
                } else {
                    ERROR("Unable to read persistent property file %s errno: %d\n", path, errno);
                }
                close(fd);
            } else {
                ERROR("Unable to open persistent property file %s errno: %d\n", path, errno);
            }
        }
        closedir(dir);
    } else {
        ERROR("Unable to open persistent property directory %s errno: %d\n", PERSISTENT_PROPERTY_DIR, errno);
    }

    persistent_properties_loaded = 1;
}

static void write_persistent_property(const char *name, const char *value)
{
    const char *tempPath = PERSISTENT_PROPERTY_DIR "/.temp";
    char path[PATH_MAX];
    int fd, length;

    snprintf(path, sizeof(path), "%s/%s", PERSISTENT_PROPERTY_DIR, name);

    fd = open(tempPath, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
        ERROR("Unable to write persistent property to temp file %s errno: %d\n", tempPath, errno);
        return;
    }
    write(fd, value, strlen(value));
    close(fd);

    if (rename(tempPath, path)) {
        unlink(tempPath);
        ERROR("Unable to rename persistent property file %s to %s\n", tempPath, path);
    }
}

int property_init()
{
    int rc;

    rc = __system_properties_init();
    if (rc) return -1;

    load_properties_from_file(PROP_PATH_SYSTEM_BUILD);
    load_persistent_properties();
}

const char* property_get(const char *name)
{
    prop_area *pa;
    const prop_info *pi;
    int len = 0;

    pi =__system_property_find(name);

    if(pi != 0) {
        return pi->value;
    } else {
        return 0;
    }
}

int property_set(const char *name, char *value)
{
    prop_area *pa;
    prop_info *pi;

    int namelen = strlen(name);
    int valuelen = strlen(value);

    if(namelen >= PROP_NAME_MAX) return -1;
    if(valuelen >= PROP_VALUE_MAX) return -1;
    if(namelen < 1) return -1;

    pi = (prop_info*) __system_property_find(name);

    if(pi != 0) {
        /* ro.* properties may NEVER be modified once set */
        if(!strncmp(name, "ro.", 3)) return -1;

        pa = __system_property_area__;
        update_prop_info(pi, value, valuelen);
        pa->serial++;
        futex_wake(&pa->serial, INT32_MAX);
    } else {
        pa = __system_property_area__;
        if(pa->count == PA_COUNT_MAX) return -1;

        pi = pa_info_array + pa->count;
        pi->serial = (valuelen << 24);
        memcpy(pi->name, name, namelen + 1);
        memcpy(pi->value, value, valuelen + 1);

        pa->toc[pa->count] =
            (namelen << 24) | (((unsigned) pi) - ((unsigned) pa));

        pa->count++;
        pa->serial++;
        futex_wake(&pa->serial, INT32_MAX);
    }

    if (persistent_properties_loaded &&
            strncmp("persist.", name, strlen("persist.")) == 0) {
        /*
         * Don't write properties to disk until after we have read all default properties
         * to prevent them from being overwritten by default values.
         */
        write_persistent_property(name, value);
    }

    return 0;
}

int property_list(void (*propfn)(const char *key, const char *value, void *cookie),
                  void *cookie)
{
    char name[PROP_NAME_MAX];
    char value[PROP_VALUE_MAX];
    const prop_info *pi;
    unsigned n;

    for(n = 0; (pi = __system_property_find_nth(n)); n++) {
        __system_property_read(pi, name, value);
        propfn(name, value, cookie);
    }
    return 0;
}
