// ----------------------------------------------------------------------------
// Copyright 2016-2018 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#include "storage_helper.h"

StorageHelper::StorageHelper(BlockDevice *bd, FileSystem *fs)
    : _bd(bd), _fs(fs)
{

}

int StorageHelper::init() {
    static bool init_done = false;
    int status = 0;

    if(!init_done) {
        BlockDevice *bd = _bd;
        if (bd) {
            status = bd->init();

            if (status != BD_ERROR_OK) {
                printf("[SMCC] StorageHelper::init() - bd->init() failed with %d\n", status);
                return -1;
            }

#if (MCC_PLATFORM_PARTITION_MODE == 1)
            // store partition size
            mcc_platform_storage_size = bd->size();
#endif
            // printf("[SMCC] StorageHelper::init() - bd->size() = %llu\n", bd->size());
            // printf("[SMCC] StorageHelper::init() - BlockDevice init OK\n");
        }

#if (MCC_PLATFORM_PARTITION_MODE == 1)
#if (NUMBER_OF_PARTITIONS > 0)
        status = init_and_mount_partition(&fs1, &part1, PRIMARY_PARTITION_NUMBER, ((const char*) MOUNT_POINT_PRIMARY+1));
        if (status != 0) {
#if (MCC_PLATFORM_AUTO_PARTITION == 1)
            status = create_partitions();
            if (status != 0) {
                return status;
            }
#else
            printf("[SMCC] StorageHelper::init() - primary partition init fail!!!\n");
            return status;
#endif
        }

#if (NUMBER_OF_PARTITIONS == 2)
        status = init_and_mount_partition(&fs2, &part2, SECONDARY_PARTITION_NUMBER, ((const char*) MOUNT_POINT_SECONDARY+1));
        if (status != 0) {
#if (MCC_PLATFORM_AUTO_PARTITION == 1)
            status = create_partitions();
            if (status != 0) {
                return status;
            }
#else
            printf("[SMCC] StorageHelper::init() - secondary partition init fail!!!\n");
            return status;
#endif
        }
#endif // (NUMBER_OF_PARTITIONS == 2)
#if (NUMBER_OF_PARTITIONS > 2)
#error "Invalid number of partitions!!!"
#endif
#endif // (NUMBER_OF_PARTITIONS > 0)
#else  // Else for #if (MCC_PLATFORM_PARTITION_MODE == 1)
    fs1 = _fs;
    part1 = bd;                   /* required for mcc_platform_reformat_storage */
    status = test_filesystem(fs1, bd);
    if (status != 0) {
        printf("[SMCC] Formatting ...\n");
        status = reformat_partition(fs1, bd);
        if (status != 0) {
            printf("[SMCC] Formatting failed with 0x%X !!!\n", status);
            return status;
        }
    }
#endif // MCC_PLATFORM_PARTITION_MODE
        init_done = true;
    }
    else {
        printf("StorageHelper::init() - init already done\n");
    }

    return status;
}

int StorageHelper::sotp_init(void)
{
    int status = FCC_STATUS_SUCCESS;
// Include this only for Developer mode and a device which doesn't have in-built TRNG support.
#if MBED_CONF_APP_DEVELOPER_MODE == 1
#ifdef PAL_USER_DEFINED_CONFIGURATION
#if !PAL_USE_HW_TRNG
    status = fcc_entropy_set(MBED_CLOUD_DEV_ENTROPY, FCC_ENTROPY_SIZE);

    if (status != FCC_STATUS_SUCCESS && status != FCC_STATUS_ENTROPY_ERROR) {
        printf("[SMCC] fcc_entropy_set failed with status %d! - exit\n", status);
        fcc_finalize();
        return status;
    }
#endif // PAL_USE_HW_TRNG = 0
/* Include this only for Developer mode. The application will use fixed RoT to simplify user-experience with the application.
* With this change the application be reflashed/SOTP can be erased safely without invalidating the application credentials.
*/
    status = fcc_rot_set(MBED_CLOUD_DEV_ROT, FCC_ROT_SIZE);

    if (status != FCC_STATUS_SUCCESS && status != FCC_STATUS_ROT_ERROR) {
        printf("[SMCC] fcc_rot_set failed with status %d! - exit\n", status);
        fcc_finalize();
    } else {
        // We can return SUCCESS here as preexisting RoT/Entropy is expected flow.
        printf("[SMCC] Using hardcoded Root of Trust, not suitable for production use.\n");
        status = FCC_STATUS_SUCCESS;
    }
#endif // PAL_USER_DEFINED_CONFIGURATION
#endif // #if MBED_CONF_APP_DEVELOPER_MODE == 1
    return status;
}


int StorageHelper::reformat_storage(void) {
    int status = -1;

    if (_bd) {
#if (NUMBER_OF_PARTITIONS > 0)
        status = reformat_partition(fs1, part1);
        if (status != 0) {
            printf("[SMCC] Formatting primary partition failed with 0x%X !!!\n", status);
            return status;
        }
#if (NUMBER_OF_PARTITIONS == 2)
        status = reformat_partition(fs2, part2);
        if (status != 0) {
            printf("[SMCC] Formatting secondary partition failed with 0x%X !!!\n", status);
            return status;
        }
#endif
#if (NUMBER_OF_PARTITIONS > 2)
#error "Invalid number of partitions!!!"
#endif
#endif

#if NUMBER_OF_PARTITIONS == 0
        status = _fs->reformat(_bd);
#endif
    }

    printf("[SMCC] Storage reformatted (%d)\n", status);

    return status;
}

#if (MCC_PLATFORM_PARTITION_MODE == 1)
// bd must be initialized before calling this function.
int StorageHelper::init_and_mount_partition(FileSystem **fs, BlockDevice** part, int number_of_partition, const char* mount_point) {
    int status;

    // Init fs only once.
    if (&(**fs) == NULL) {
        if (&(**part) == NULL) {
            *part = new MBRBlockDevice(bd, number_of_partition);
        }
        status = (**part).init();
        if (status != 0) {
            (**part).deinit();
            printf("[SMCC] Init of partition %d fail !!!\n", number_of_partition);
            return status;
        }
        /* This next change mean that filesystem will be FAT. */
        *fs = new FATFileSystem(mount_point, &(**part));  /* this also mount fs. */
    }
    // re-init and format.
    else {
        status = (**part).init();
        if (status != 0) {
            (**part).deinit();
            printf("[SMCC] Init of partition %d fail !!!\n", number_of_partition);
            return status;
        }

        printf("[SMCC] Formatting partition %d ...\n", number_of_partition);
        status = reformat_partition(&(**fs), &(**part));
        if (status != 0) {
            printf("[SMCC] Formatting partition %d failed with 0x%X !!!\n", number_of_partition, status);
            return status;
        }
    }

    status = test_filesystem(&(**fs), &(**part));
    if (status != 0) {
        printf("[SMCC] Formatting partition %d ...\n", number_of_partition);
        status = reformat_partition(&(**fs), &(**part));
        if (status != 0) {
            printf("[SMCC] Formatting partition %d failed with 0x%X !!!\n", number_of_partition, status);
            return status;
        }
    }

    return status;
}
#endif

int StorageHelper::reformat_partition(FileSystem *fs, BlockDevice* part) {
    int status;

    printf("[SMCC] reformat_partition\n");
    status = fs->reformat(part);
    if (status != 0) {
        printf("[SMCC] Reformat partition failed with error %d\n", status);
    }

    return status;
}

/* help function for testing filesystem availbility by umount and
* mount filesystem again.
* */
int StorageHelper::test_filesystem(FileSystem *fs, BlockDevice* part) {
    // unmount
    int status = fs->unmount();
    if (status != 0) {
        printf("[SMCC] test_filesystem() - unmount fail %d.\n", status);
        return -1;
    }
    // mount again
    status = fs->mount(part);
    if (status != 0) {
        printf("[SMCC] test_filesystem() - mount fail %d.\n", status);
        return -1;
    }
    return status;
}

// create partitions, initialize and mount partitions
#if ((MCC_PLATFORM_PARTITION_MODE == 1) && (MCC_PLATFORM_AUTO_PARTITION == 1))
static int StorageHelper::create_partitions(void) {
    int status;

#if (NUMBER_OF_PARTITIONS > 0)
    if (mcc_platform_storage_size < PRIMARY_PARTITION_SIZE) {
        printf("[SMCC] create_partitions PRIMARY_PARTITION_SIZE too large!!! Storage's size is %" PRIu64 \
                " and PRIMARY_PARTITION_SIZE is %" PRIu64 "\n",
                (uint64_t)mcc_platform_storage_size, (uint64_t)PRIMARY_PARTITION_SIZE);
        assert(0);
    }

    status = MBRBlockDevice::partition(bd, PRIMARY_PARTITION_NUMBER, 0x83, PRIMARY_PARTITION_START, PRIMARY_PARTITION_START + PRIMARY_PARTITION_SIZE);
    printf("[SMCC] Creating primary partition ...\n");
    if (status != 0) {
        printf("[SMCC] Creating primary partition fail 0x%X !!!\n", status);
        return status;
    }

    // init and format partition 1
    status = init_and_mount_partition(&fs1, &part1, PRIMARY_PARTITION_NUMBER, ((const char*) MOUNT_POINT_PRIMARY+1));
    if (status != 0) {
        return status;
    }
#if (NUMBER_OF_PARTITIONS == 2)
    // use cast (uint64_t) for fixing compile warning.
    if (mcc_platform_storage_size < ((uint64_t)PRIMARY_PARTITION_SIZE + (uint64_t)SECONDARY_PARTITION_SIZE)) {
        printf("[SMCC] create_partitions (PRIMARY_PARTITION_SIZE+SECONDARY_PARTITION_SIZE) too large!!! Storage's size is %" PRIu64 \
                " and (PRIMARY_PARTITION_SIZE+SECONDARY_PARTITION_SIZE) %" PRIu64 "\n",
                (uint64_t)mcc_platform_storage_size, (uint64_t)(PRIMARY_PARTITION_SIZE+SECONDARY_PARTITION_SIZE));
        assert(0);
    }

    // use cast (uint64_t) for fixing compile warning.
    status = MBRBlockDevice::partition(bd, SECONDARY_PARTITION_NUMBER, 0x83, SECONDARY_PARTITION_START, (uint64_t) SECONDARY_PARTITION_START + (uint64_t) SECONDARY_PARTITION_SIZE);
    printf("[SMCC] Creating secondary partition ...\n");
    if (status != 0) {
        printf("[SMCC] Creating secondary partition fail 0x%X !!!\n", status);
        return status;
    }

    // init and format partition 2
    status = init_and_mount_partition(&fs2, &part2, SECONDARY_PARTITION_NUMBER, ((const char*) MOUNT_POINT_SECONDARY+1));
    if (status != 0) {
        return status;
    }
#endif
#if (NUMBER_OF_PARTITIONS > 2)
#error "Invalid number of partitions!!!"
#endif
#endif // (NUMBER_OF_PARTITIONS > 0)
    return status;
}
#endif // ((MCC_PLATFORM_PARTITION_MODE == 1) && (MCC_PLATFORM_AUTO_PARTITION == 1))
