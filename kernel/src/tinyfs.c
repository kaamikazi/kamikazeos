#include "tinyfs.h"
#include "ata.h"
#include "errors.h"
#include "serial.h"
#include "string.h"

#define TINYFS_VERSION 1u
#define TABLE_SECTORS (TINYFS_LBA_TABLE_END - TINYFS_LBA_TABLE_START + 1)
#define DATA_SECTORS (TINYFS_LBA_DATA_END - TINYFS_LBA_DATA_START + 1)
#define FILE_ENTRY_SIZE 48u

#define SB_MAGIC 0
#define SB_VERSION 4
#define SB_CHECKSUM 8
#define SB_FILE_COUNT 12
#define SB_DATA_START 16
#define SB_MAX_FILES 20
#define SB_MAX_FILE_SIZE 24
#define SB_TABLE_START 28
#define SB_TABLE_SECTORS 32
#define SB_DATA_END 36

#define FE_USED 0
#define FE_FLAGS 1
#define FE_NAME 4
#define FE_SIZE 28
#define FE_START_LBA 32
#define FE_SECTOR_COUNT 36
#define FE_CHECKSUM 40

static struct tinyfs_file files[TINYFS_MAX_FILES];
static struct tinyfs_status_info status;
static uint8_t sector[ATA_SECTOR_SIZE];
static uint8_t table_bytes[TABLE_SECTORS * ATA_SECTOR_SIZE];
static uint8_t file_bytes[TINYFS_MAX_FILE_SIZE];

static void put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t checksum_bytes(const uint8_t *data, uint32_t length)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static int lba_in_range(uint32_t lba)
{
    return lba >= TINYFS_LBA_SUPERBLOCK && lba <= TINYFS_LBA_DATA_END;
}

static int fs_read_sector(uint32_t lba, uint8_t *buffer)
{
    return lba_in_range(lba) && ata_read_sector(lba, buffer);
}

static int fs_write_sector(uint32_t lba, const uint8_t *buffer)
{
    return lba_in_range(lba) && ata_write_sector(lba, buffer);
}

static void set_error(const char *value)
{
    status.last_error = value;
}

static int sector_is_zero(const uint8_t *buffer)
{
    for (uint32_t i = 0; i < ATA_SECTOR_SIZE; i++)
        if (buffer[i] != 0)
            return 0;
    return 1;
}

static int valid_name(const char *name)
{
    uint32_t len = (uint32_t)kstrlen(name);
    if (len == 0 || len > TINYFS_NAME_MAX)
        return 0;
    if (kstrcmp(name, ".") == 0 || kstrcmp(name, "..") == 0)
        return 0;
    for (uint32_t i = 0; i < len; i++) {
        char c = name[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok)
            return 0;
    }
    return 1;
}

static int find_file(const char *name)
{
    for (uint32_t i = 0; i < TINYFS_MAX_FILES; i++)
        if (files[i].used && kstrcmp(files[i].name, name) == 0)
            return (int)i;
    return -1;
}

static int find_free_entry(void)
{
    for (uint32_t i = 0; i < TINYFS_MAX_FILES; i++)
        if (!files[i].used)
            return (int)i;
    return -1;
}

static void serialize_file(uint8_t *dst, const struct tinyfs_file *file)
{
    kmemset(dst, 0, FILE_ENTRY_SIZE);
    dst[FE_USED] = file->used;
    dst[FE_FLAGS] = file->flags;
    for (uint32_t i = 0; i < TINYFS_NAME_MAX && file->name[i]; i++)
        dst[FE_NAME + i] = (uint8_t)file->name[i];
    put_u32(dst + FE_SIZE, file->size);
    put_u32(dst + FE_START_LBA, file->start_lba);
    put_u32(dst + FE_SECTOR_COUNT, file->sector_count);
    put_u32(dst + FE_CHECKSUM, file->checksum);
}

static void deserialize_file(struct tinyfs_file *file, const uint8_t *src)
{
    kmemset(file, 0, sizeof(*file));
    file->used = src[FE_USED];
    file->flags = src[FE_FLAGS];
    for (uint32_t i = 0; i < TINYFS_NAME_MAX; i++)
        file->name[i] = (char)src[FE_NAME + i];
    file->name[TINYFS_NAME_MAX] = '\0';
    file->size = get_u32(src + FE_SIZE);
    file->start_lba = get_u32(src + FE_START_LBA);
    file->sector_count = get_u32(src + FE_SECTOR_COUNT);
    file->checksum = get_u32(src + FE_CHECKSUM);
}

static int write_superblock(void)
{
    kmemset(sector, 0, sizeof(sector));
    sector[0] = 'K'; sector[1] = 'M'; sector[2] = 'Z'; sector[3] = 'F';
    put_u32(sector + SB_VERSION, TINYFS_VERSION);
    put_u32(sector + SB_FILE_COUNT, status.file_count);
    put_u32(sector + SB_DATA_START, TINYFS_LBA_DATA_START);
    put_u32(sector + SB_MAX_FILES, TINYFS_MAX_FILES);
    put_u32(sector + SB_MAX_FILE_SIZE, TINYFS_MAX_FILE_SIZE);
    put_u32(sector + SB_TABLE_START, TINYFS_LBA_TABLE_START);
    put_u32(sector + SB_TABLE_SECTORS, TABLE_SECTORS);
    put_u32(sector + SB_DATA_END, TINYFS_LBA_DATA_END);
    put_u32(sector + SB_CHECKSUM, 0);
    put_u32(sector + SB_CHECKSUM, checksum_bytes(sector, sizeof(sector)));
    return fs_write_sector(TINYFS_LBA_SUPERBLOCK, sector);
}

static int valid_superblock(const uint8_t *buffer)
{
    if (buffer[0] != 'K' || buffer[1] != 'M' ||
        buffer[2] != 'Z' || buffer[3] != 'F')
        return 0;
    uint32_t stored = get_u32(buffer + SB_CHECKSUM);
    uint8_t copy[ATA_SECTOR_SIZE];
    memcpy(copy, buffer, sizeof(copy));
    put_u32(copy + SB_CHECKSUM, 0);
    if (stored != checksum_bytes(copy, sizeof(copy)))
        return 0;
    return get_u32(buffer + SB_VERSION) == TINYFS_VERSION &&
           get_u32(buffer + SB_DATA_START) == TINYFS_LBA_DATA_START &&
           get_u32(buffer + SB_MAX_FILES) == TINYFS_MAX_FILES &&
           get_u32(buffer + SB_MAX_FILE_SIZE) == TINYFS_MAX_FILE_SIZE &&
           get_u32(buffer + SB_TABLE_START) == TINYFS_LBA_TABLE_START &&
           get_u32(buffer + SB_TABLE_SECTORS) == TABLE_SECTORS &&
           get_u32(buffer + SB_DATA_END) == TINYFS_LBA_DATA_END &&
           get_u32(buffer + SB_FILE_COUNT) <= TINYFS_MAX_FILES;
}

static int write_table(void)
{
    kmemset(table_bytes, 0, sizeof(table_bytes));
    for (uint32_t i = 0; i < TINYFS_MAX_FILES; i++)
        serialize_file(table_bytes + i * FILE_ENTRY_SIZE, &files[i]);
    for (uint32_t i = 0; i < TABLE_SECTORS; i++)
        if (!fs_write_sector(TINYFS_LBA_TABLE_START + i,
                             table_bytes + i * ATA_SECTOR_SIZE))
            return 0;
    return 1;
}

static int load_table(void)
{
    for (uint32_t i = 0; i < TABLE_SECTORS; i++)
        if (!fs_read_sector(TINYFS_LBA_TABLE_START + i,
                            table_bytes + i * ATA_SECTOR_SIZE))
            return 0;

    uint8_t used_map[DATA_SECTORS];
    kmemset(used_map, 0, sizeof(used_map));
    uint32_t count = 0;
    for (uint32_t i = 0; i < TINYFS_MAX_FILES; i++) {
        deserialize_file(&files[i], table_bytes + i * FILE_ENTRY_SIZE);
        struct tinyfs_file *file = &files[i];
        if (!file->used)
            continue;
        if (!valid_name(file->name) || file->size > TINYFS_MAX_FILE_SIZE ||
            file->sector_count > 4 ||
            file->sector_count != (file->size + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE ||
            (file->sector_count == 0 && file->start_lba != 0) ||
            (file->sector_count &&
             (file->start_lba < TINYFS_LBA_DATA_START ||
              file->start_lba + file->sector_count - 1 > TINYFS_LBA_DATA_END)))
            return 0;
        for (uint32_t s = 0; s < file->sector_count; s++) {
            uint32_t bit = file->start_lba - TINYFS_LBA_DATA_START + s;
            if (used_map[bit])
                return 0;
            used_map[bit] = 1;
        }
        for (uint32_t j = 0; j < i; j++)
            if (files[j].used && kstrcmp(files[j].name, file->name) == 0)
                return 0;
        count++;
    }
    return count == status.file_count;
}

static int require_ready(void)
{
    if (!status.available) {
        set_error("disk_unavailable");
        error_log(SEV_WARNING, "TFS-ERR-001", "TinyFS",
                  "TinyFS operation refused without ATA disk",
                  "attach the recovery disk and reboot");
        return 0;
    }
    if (!status.formatted) {
        set_error("not_formatted");
        error_log(SEV_WARNING, "TFS-ERR-002", "TinyFS",
                  "TinyFS operation refused before format",
                  "run fs-format confirm");
        return 0;
    }
    return 1;
}

static int require_valid_name(const char *name)
{
    if (valid_name(name))
        return 1;
    set_error("invalid_filename");
    error_log(SEV_WARNING, "TFS-ERR-004", "TinyFS",
              "TinyFS rejected invalid filename",
              "use 1 to 24 letters digits dot dash or underscore");
    return 0;
}

void tinyfs_init(void)
{
    kmemset(files, 0, sizeof(files));
    status.available = ata_available();
    status.formatted = 0;
    status.file_count = 0;
    status.max_files = TINYFS_MAX_FILES;
    status.data_start_lba = TINYFS_LBA_DATA_START;
    status.max_file_size = TINYFS_MAX_FILE_SIZE;
    set_error("none");

    if (!status.available) {
        set_error("disk_unavailable");
        error_log(SEV_WARNING, "TFS-WARN-001", "TinyFS",
                  "TinyFS unavailable because ATA disk is missing",
                  "kernel continues without filesystem support");
        return;
    }
    if (!fs_read_sector(TINYFS_LBA_SUPERBLOCK, sector)) {
        set_error("superblock_read_failed");
        error_log(SEV_WARNING, "TFS-WARN-002", "TinyFS",
                  "TinyFS superblock could not be read",
                  "filesystem disabled without affecting recovery data");
        return;
    }
    if (sector_is_zero(sector)) {
        set_error("not_formatted");
        error_log(SEV_INFO, "TFS-001", "TinyFS",
                  "TinyFS area is not formatted",
                  "run fs-format confirm to initialize LBA 64 through 127");
        return;
    }
    if (!valid_superblock(sector)) {
        set_error("invalid_superblock");
        error_log(SEV_WARNING, "TFS-WARN-003", "TinyFS",
                  "TinyFS superblock validation failed",
                  "do not trust files - reformat only with human confirmation");
        return;
    }

    status.file_count = get_u32(sector + SB_FILE_COUNT);
    if (!load_table()) {
        kmemset(files, 0, sizeof(files));
        status.file_count = 0;
        set_error("invalid_file_table");
        error_log(SEV_WARNING, "TFS-WARN-004", "TinyFS",
                  "TinyFS file table validation failed",
                  "filesystem disabled without touching recovery sectors");
        return;
    }
    status.formatted = 1;
    set_error("none");
    error_log(SEV_INFO, "TFS-000", "TinyFS",
              "TinyFS metadata loaded and validated",
              "fixed LBA range 64 through 127 only");
}

int tinyfs_format(void)
{
    if (!status.available) {
        set_error("disk_unavailable");
        error_log(SEV_WARNING, "TFS-ERR-001", "TinyFS",
                  "TinyFS format refused without ATA disk",
                  "attach the recovery disk and reboot");
        return 0;
    }

    kmemset(sector, 0, sizeof(sector));
    for (uint32_t lba = TINYFS_LBA_SUPERBLOCK; lba <= TINYFS_LBA_DATA_END; lba++) {
        if (!fs_write_sector(lba, sector)) {
            status.formatted = 0;
            set_error("format_write_failed");
            error_log(SEV_ERROR, "TFS-ERR-003", "TinyFS",
                      "TinyFS format failed inside reserved range",
                      "recovery sectors were not targeted");
            return 0;
        }
    }

    kmemset(files, 0, sizeof(files));
    status.file_count = 0;
    if (!write_table() || !write_superblock()) {
        status.formatted = 0;
        set_error("format_metadata_failed");
        error_log(SEV_ERROR, "TFS-ERR-003", "TinyFS",
                  "TinyFS format metadata write failed",
                  "retry format after checking the disk");
        return 0;
    }
    status.formatted = 1;
    set_error("none");
    error_log(SEV_INFO, "TFS-FMT-000", "TinyFS",
              "TinyFS formatted LBA 64 through 127",
              "RecoveryStore and package sectors were not touched");
    return 1;
}

uint32_t tinyfs_list(const struct tinyfs_file **out, uint32_t capacity)
{
    if (!out || !status.formatted)
        return 0;
    uint32_t emitted = 0;
    for (uint32_t i = 0; i < TINYFS_MAX_FILES && emitted < capacity; i++)
        if (files[i].used)
            out[emitted++] = &files[i];
    return emitted;
}

int tinyfs_create(const char *name)
{
    if (!require_ready())
        return 0;
    if (!require_valid_name(name))
        return 0;
    if (find_file(name) >= 0) {
        set_error("file_exists");
        error_log(SEV_WARNING, "TFS-ERR-005", "TinyFS",
                  "TinyFS create refused duplicate filename",
                  "choose another name or delete the existing file");
        return 0;
    }
    int slot = find_free_entry();
    if (slot < 0) {
        set_error("file_table_full");
        error_log(SEV_WARNING, "TFS-ERR-006", "TinyFS",
                  "TinyFS file table is full",
                  "delete an unused file before creating another");
        return 0;
    }

    kmemset(&files[slot], 0, sizeof(files[slot]));
    files[slot].used = 1;
    kstrcpy(files[slot].name, name, sizeof(files[slot].name));
    files[slot].checksum = checksum_bytes((const uint8_t *)"", 0);
    status.file_count++;
    if (!write_table() || !write_superblock()) {
        kmemset(&files[slot], 0, sizeof(files[slot]));
        status.file_count--;
        set_error("metadata_write_failed");
        error_log(SEV_ERROR, "TFS-ERR-007", "TinyFS",
                  "TinyFS could not persist new file metadata",
                  "file creation was rolled back in memory");
        return 0;
    }
    set_error("none");
    error_log(SEV_INFO, "TFS-CREATE-000", "TinyFS",
              "TinyFS file created", "use write to store bounded text");
    return 1;
}

static int allocate_extent(uint32_t sectors_needed, int exclude_index,
                           uint32_t *start_lba)
{
    if (sectors_needed == 0) {
        *start_lba = 0;
        return 1;
    }
    uint8_t used_map[DATA_SECTORS];
    kmemset(used_map, 0, sizeof(used_map));
    for (uint32_t i = 0; i < TINYFS_MAX_FILES; i++) {
        if (!files[i].used || (int)i == exclude_index)
            continue;
        for (uint32_t s = 0; s < files[i].sector_count; s++)
            used_map[files[i].start_lba - TINYFS_LBA_DATA_START + s] = 1;
    }
    for (uint32_t first = 0; first + sectors_needed <= DATA_SECTORS; first++) {
        int free = 1;
        for (uint32_t s = 0; s < sectors_needed; s++)
            if (used_map[first + s]) { free = 0; break; }
        if (free) {
            *start_lba = TINYFS_LBA_DATA_START + first;
            return 1;
        }
    }
    return 0;
}

int tinyfs_write_text(const char *name, const char *text)
{
    if (!require_ready())
        return 0;
    if (!require_valid_name(name))
        return 0;
    int index = find_file(name);
    if (index < 0) {
        set_error("file_not_found");
        error_log(SEV_WARNING, "TFS-ERR-008", "TinyFS",
                  "TinyFS write target was not found",
                  "create the file with touch first");
        return 0;
    }
    uint32_t length = (uint32_t)kstrlen(text);
    if (length > TINYFS_MAX_FILE_SIZE) {
        set_error("file_too_large");
        error_log(SEV_WARNING, "TFS-ERR-009", "TinyFS",
                  "TinyFS rejected oversized write",
                  "maximum file size is 2048 bytes");
        return 0;
    }
    uint32_t sectors_needed = (length + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;
    uint32_t start_lba;
    if (!allocate_extent(sectors_needed, index, &start_lba)) {
        set_error("data_area_full");
        error_log(SEV_WARNING, "TFS-ERR-010", "TinyFS",
                  "TinyFS data area has no contiguous extent",
                  "delete files or write smaller content");
        return 0;
    }

    kmemset(file_bytes, 0, sizeof(file_bytes));
    memcpy(file_bytes, text, length);
    for (uint32_t s = 0; s < sectors_needed; s++) {
        if (!fs_write_sector(start_lba + s, file_bytes + s * ATA_SECTOR_SIZE)) {
            set_error("data_write_failed");
            error_log(SEV_ERROR, "TFS-ERR-011", "TinyFS",
                      "TinyFS data sector write failed",
                      "file metadata was left unchanged");
            return 0;
        }
    }

    struct tinyfs_file old = files[index];
    files[index].size = length;
    files[index].start_lba = start_lba;
    files[index].sector_count = sectors_needed;
    files[index].checksum = checksum_bytes(file_bytes, length);
    if (!write_table()) {
        files[index] = old;
        set_error("metadata_write_failed");
        error_log(SEV_ERROR, "TFS-ERR-007", "TinyFS",
                  "TinyFS could not persist write metadata",
                  "previous in-memory file metadata was restored");
        return 0;
    }
    set_error("none");
    error_log(SEV_INFO, "TFS-WRITE-000", "TinyFS",
              "TinyFS text write completed",
              "bounded checksum-protected file data persisted");
    return 1;
}

int tinyfs_read_text(const char *name, char *out,
                     uint32_t capacity, uint32_t *size_out)
{
    if (!require_ready() || !out || capacity == 0)
        return 0;
    if (!require_valid_name(name))
        return 0;
    int index = find_file(name);
    if (index < 0) {
        set_error("file_not_found");
        error_log(SEV_WARNING, "TFS-ERR-008", "TinyFS",
                  "TinyFS read target was not found",
                  "run ls to inspect available files");
        return 0;
    }
    const struct tinyfs_file *file = &files[index];
    if (capacity <= file->size) {
        set_error("read_buffer_too_small");
        error_log(SEV_ERROR, "TFS-ERR-012", "TinyFS",
                  "TinyFS read buffer was too small",
                  "kernel caller must provide file size plus terminator");
        return 0;
    }
    kmemset(file_bytes, 0, sizeof(file_bytes));
    for (uint32_t s = 0; s < file->sector_count; s++)
        if (!fs_read_sector(file->start_lba + s, file_bytes + s * ATA_SECTOR_SIZE)) {
            set_error("data_read_failed");
            error_log(SEV_ERROR, "TFS-ERR-013", "TinyFS",
                      "TinyFS data sector read failed",
                      "file contents were not returned");
            return 0;
        }
    if (checksum_bytes(file_bytes, file->size) != file->checksum) {
        set_error("checksum_mismatch");
        error_log(SEV_ERROR, "TFS-ERR-014", "TinyFS",
                  "TinyFS file checksum mismatch",
                  "do not trust the file contents");
        return 0;
    }
    memcpy(out, file_bytes, file->size);
    out[file->size] = '\0';
    if (size_out)
        *size_out = file->size;
    set_error("none");
    error_log(SEV_INFO, "TFS-READ-000", "TinyFS",
              "TinyFS text read completed",
              "checksum matched stored metadata");
    return 1;
}

int tinyfs_delete(const char *name)
{
    if (!require_ready())
        return 0;
    if (!require_valid_name(name))
        return 0;
    int index = find_file(name);
    if (index < 0) {
        set_error("file_not_found");
        error_log(SEV_WARNING, "TFS-ERR-008", "TinyFS",
                  "TinyFS delete target was not found",
                  "run ls to inspect available files");
        return 0;
    }

    struct tinyfs_file old = files[index];
    kmemset(sector, 0, sizeof(sector));
    for (uint32_t s = 0; s < old.sector_count; s++) {
        if (!fs_write_sector(old.start_lba + s, sector)) {
            set_error("delete_wipe_failed");
            error_log(SEV_ERROR, "TFS-ERR-015", "TinyFS",
                      "TinyFS could not wipe deleted file data",
                      "file metadata was left intact");
            return 0;
        }
    }
    kmemset(&files[index], 0, sizeof(files[index]));
    status.file_count--;
    if (!write_table() || !write_superblock()) {
        files[index] = old;
        status.file_count++;
        set_error("metadata_write_failed");
        error_log(SEV_ERROR, "TFS-ERR-007", "TinyFS",
                  "TinyFS could not persist deletion metadata",
                  "previous in-memory metadata was restored");
        return 0;
    }
    set_error("none");
    error_log(SEV_INFO, "TFS-DELETE-000", "TinyFS",
              "TinyFS file deleted and data sectors wiped",
              "freed sectors remain inside TinyFS range");
    return 1;
}

const struct tinyfs_status_info *tinyfs_status(void)
{
    return &status;
}

static void serial_dec(uint32_t value)
{
    char buffer[12];
    kitoa(value, buffer, 10);
    serial_print(buffer);
}

void tinyfs_export_report(void)
{
    serial_print("[TINYFS]\n");
    serial_print("available="); serial_print(status.available ? "yes" : "no");
    serial_print("\nformatted="); serial_print(status.formatted ? "yes" : "no");
    serial_print("\nfile_count="); serial_dec(status.file_count);
    serial_print("\nmax_files="); serial_dec(status.max_files);
    serial_print("\ndata_start_lba="); serial_dec(status.data_start_lba);
    serial_print("\nmax_file_size="); serial_dec(status.max_file_size);
    serial_print("\nlast_error="); serial_print(status.last_error);
    serial_print("\n");
    serial_print("[TINYFS_FILES]\n");
    for (uint32_t i = 0; i < TINYFS_MAX_FILES; i++) {
        if (!files[i].used)
            continue;
        serial_print("name="); serial_print(files[i].name);
        serial_print(";size="); serial_dec(files[i].size);
        serial_print(";start_lba="); serial_dec(files[i].start_lba);
        serial_print(";sector_count="); serial_dec(files[i].sector_count);
        serial_print(";checksum="); serial_dec(files[i].checksum);
        serial_print(";flags="); serial_dec(files[i].flags);
        serial_print("\n");
    }
}
