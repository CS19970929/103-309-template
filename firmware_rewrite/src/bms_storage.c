#include "bms_app.h"

#include <string.h>

static uint32_t crc32_words(const bms_snapshot_t *snapshot)
{
    const uint8_t *bytes = (const uint8_t *)snapshot;
    const size_t crc_offset = offsetof(bms_snapshot_t, crc);
    uint32_t crc = 2166136261u;
    size_t i;

    for (i = 0u; i < crc_offset; ++i) {
        crc ^= bytes[i];
        crc *= 16777619u;
    }

    return crc;
}

void bms_storage_init(bms_storage_t *storage)
{
    memset(storage, 0xFF, sizeof(*storage));
}

bool bms_storage_validate(const bms_snapshot_t *snapshot)
{
    bms_snapshot_t copy;

    if (snapshot == NULL ||
        snapshot->magic != 0x42534F43ul ||
        snapshot->version != 2u ||
        snapshot->soc > 100u ||
        snapshot->display_soc > 100u ||
        snapshot->soh < 80u ||
        snapshot->soh > 100u ||
        snapshot->cap_full_as10 == 0u ||
        snapshot->cap_now_as10 > snapshot->cap_full_as10) {
        return false;
    }

    copy = *snapshot;
    copy.crc = 0u;
    return crc32_words(&copy) == snapshot->crc;
}

bool bms_storage_load_latest(const bms_storage_t *storage, bms_snapshot_t *snapshot)
{
    const bool slot0_valid = bms_storage_validate(&storage->slot[0]);
    const bool slot1_valid = bms_storage_validate(&storage->slot[1]);

    if (!slot0_valid && !slot1_valid) {
        return false;
    }

    if (slot0_valid && (!slot1_valid || storage->slot[0].sequence >= storage->slot[1].sequence)) {
        *snapshot = storage->slot[0];
    } else {
        *snapshot = storage->slot[1];
    }

    return true;
}

bool bms_storage_save(bms_storage_t *storage, const bms_snapshot_t *snapshot)
{
    bms_snapshot_t copy = *snapshot;
    size_t target = 0u;
    const bool slot0_valid = bms_storage_validate(&storage->slot[0]);
    const bool slot1_valid = bms_storage_validate(&storage->slot[1]);

    copy.crc = 0u;
    copy.crc = crc32_words(&copy);

    if (!slot0_valid) {
        target = 0u;
    } else if (!slot1_valid) {
        target = 1u;
    } else if (storage->slot[0].sequence <= storage->slot[1].sequence) {
        target = 0u;
    } else {
        target = 1u;
    }

    storage->slot[target] = copy;
    return bms_storage_validate(&storage->slot[target]);
}
