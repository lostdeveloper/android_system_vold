/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/mount.h>

#include <linux/kdev_t.h>
#include <linux/fs.h>

#define LOG_TAG "Vold"

#include <cutils/log.h>
#include <cutils/properties.h>

#include "Ext4.h"

static char E2FSCK_PATH[] = "/system/bin/e2fsck";
static char MKEXT4FS_PATH[] = "/system/bin/make_ext4fs";

extern "C" int logwrap(int argc, const char **argv, int background);


int Ext4::doMount(const char *fsPath, const char *mountPoint, bool ro, bool remount,
        bool executable) {
    int rc;
    unsigned long flags;

    flags = MS_NOATIME | MS_NODEV | MS_NOSUID | MS_DIRSYNC;

    flags |= (executable ? 0 : MS_NOEXEC);
    flags |= (ro ? MS_RDONLY : 0);
    flags |= (remount ? MS_REMOUNT : 0);

    rc = mount(fsPath, mountPoint, "ext4", flags, NULL);

    if (rc && errno == EROFS) {
        SLOGE("%s appears to be a read only filesystem - retrying mount RO", fsPath);
        flags |= MS_RDONLY;
        rc = mount(fsPath, mountPoint, "ext4", flags, NULL);
    }

    return rc;
}

int Ext4::check(const char *fsPath) {
    bool rw = true;
    if (access(E2FSCK_PATH, X_OK)) {
        SLOGW("Skipping fs checks.\n");
        return 0;
    }

    int rc;
    do {
        const char *args[4];
        args[0] = E2FSCK_PATH;
        args[1] = "-p";
        args[2] = fsPath;
        args[3] = NULL;

        rc = logwrap(3, args, 1);
        SLOGI("E2FSCK returned %d", rc);
        if(rc == 0) {
            SLOGI("EXT4 Filesystem check completed OK.\n");
            return 0;
        }
        if(rc & 1) {
            SLOGI("EXT4 Filesystem check completed, errors corrected OK.\n");
        }
        if(rc & 2) {
            SLOGE("EXT4 Filesystem check completed, errors corrected, need reboot.\n");
        }
        if(rc & 4) {
            SLOGE("EXT4 Filesystem errors left uncorrected.\n");
        }
        if(rc & 8) {
            SLOGE("E2FSCK Operational error.\n");
            errno = EIO;
        }
        if(rc & 16) {
            SLOGE("E2FSCK Usage or syntax error.\n");
            errno = EIO;
        }
        if(rc & 32) {
            SLOGE("E2FSCK Canceled by user request.\n");
            errno = EIO;
        }
        if(rc & 128) {
            SLOGE("E2FSCK Shared library error.\n");
            errno = EIO;
        }
        if(errno == EIO) {
            return -1;
        }
    } while (0);

    return 0;
}

int Ext4::format(const char *fsPath) {
    int fd;
    const char *args[4];
    int rc;

    args[0] = MKEXT4FS_PATH;
    args[1] = "-J";
    args[2] = fsPath;
    args[3] = NULL;
    rc = logwrap(3, args, 1);

    if (rc == 0) {
        SLOGI("Filesystem (ext4) formatted OK");
        return 0;
    } else {
        SLOGE("Format (ext4) failed (unknown exit code %d)", rc);
        errno = EIO;
        return -1;
    }
    return 0;
}
