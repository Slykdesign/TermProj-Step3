#include "ext2.h"
#include "vdi.h"
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

// Function prototypes
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset);
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset);

int main() {
    struct Ext2File *ext2 = openExt2("./good-fixed-1k.vdi");
    if (!ext2) {
        return 1;
    }

    printf("Superblock:\n");
    displaySuperblock(&ext2->superblock);

    printf("\nReading first 1KB block from the file system:\n");
    uint8_t buffer[1024];
    if (fetchBlock(ext2, 0, buffer)) {
        displayBuffer(buffer, 1024, 0);
    } else {
        printf("Failed to read block.\n");
    }

    closeExt2(ext2);
    return 0;
}

// Function Definitions
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset) {
    printf("Offset: 0x%lx\n", offset);
    for (uint32_t i = skip; i < count; i += 16) {
        printf("%04x | ", i);
        for (uint32_t j = 0; j < 16 && i + j < count; j++) {
            printf("%02x ", buf[i + j]);
        }
        printf("| ");
        for (uint32_t j = 0; j < 16 && i + j < count; j++) {
            printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
        }
        printf("\n");
    }
}

void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset) {
    uint32_t step = 256;
    for (uint32_t i = 0; i < count; i += step) {
        displayBufferPage(buf, count, i, offset + i);
    }
}