#include "vdi.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

// --- Open a VDI file and initialize VDIFile struct ---
VDIFile *vdiOpen(const char *filename) {
    VDIFile *vdi = malloc(sizeof(VDIFile));    // Allocate memory for VDIFile
    if (!vdi) return NULL;                     // Return NULL if allocation failed

    vdi->fd = open(filename, O_RDWR);           // Open file read/write
    if (vdi->fd == -1) {                        // Error opening file
        perror("Error opening VDI file");
        free(vdi);
        return NULL;
    }

    // Read the first 400 bytes (header)
    if (read(vdi->fd, vdi->header, 400) != 400) {
        perror("Error reading VDI header");
        close(vdi->fd);
        free(vdi);
        return NULL;
    }

    // Extract fields needed (pageSize and totalPages)
    vdi->pageSize = *(uint32_t *)(vdi->header + 36);   // Read page size at offset 36
    vdi->totalPages = *(uint32_t *)(vdi->header + 40); // Read total pages at offset 40

    // Read the translation map (block map)
    off_t mapOffset = *(off_t *)(vdi->header + 16);    // Offset where map starts
    lseek(vdi->fd, mapOffset, SEEK_SET);               // Seek to map
    vdi->map = malloc(vdi->totalPages * sizeof(uint32_t)); // Allocate space for map
    if (!vdi->map) {
        perror("Error allocating memory for translation map");
        close(vdi->fd);
        free(vdi);
        return NULL;
    }
    read(vdi->fd, vdi->map, vdi->totalPages * sizeof(uint32_t)); // Read map

    vdi->cursor = 0;  // Initialize cursor to start
    return vdi;
}

// --- Close VDI file and free resources ---
void vdiClose(VDIFile *vdi) {
    if (vdi) {
        close(vdi->fd);       // Close file
        free(vdi->map);       // Free translation map
        free(vdi);            // Free VDIFile struct
    }
}

// --- Read data from VDI file at logical position ---
ssize_t vdiRead(VDIFile *vdi, void *buf, size_t count) {
    size_t bytesRead = 0;
    uint8_t *buffer = (uint8_t *)buf;

    while (count > 0) {
        off_t physicalOffset = vdiTranslate(vdi, vdi->cursor); // Translate logical to physical
        if (physicalOffset == -1) return -1;                   // If invalid, error

        size_t pageRemaining = vdi->pageSize - (vdi->cursor % vdi->pageSize); // How much left in current page
        size_t toRead = (count < pageRemaining) ? count : pageRemaining;     // Read either what's left or requested count

        lseek(vdi->fd, physicalOffset, SEEK_SET);   // Move to correct physical offset
        ssize_t result = read(vdi->fd, buffer, toRead); // Read data
        if (result <= 0) break;

        bytesRead += result;
        buffer += result;
        count -= result;
        vdi->cursor += result;
    }
    return bytesRead;
}

// --- Write data to VDI file at logical position ---
ssize_t vdiWrite(VDIFile *vdi, void *buf, size_t count) {
    size_t bytesWritten = 0;
    uint8_t *buffer = (uint8_t *)buf;

    while (count > 0) {
        off_t physicalOffset = vdiTranslate(vdi, vdi->cursor); // Translate logical to physical
        if (physicalOffset == -1) return bytesWritten;         // Can't write to unallocated blocks yet

        size_t pageRemaining = vdi->pageSize - (vdi->cursor % vdi->pageSize); // Remaining space in page
        size_t toWrite = (count < pageRemaining) ? count : pageRemaining;     // How much we can write now

        lseek(vdi->fd, physicalOffset, SEEK_SET);  // Seek to correct position
        ssize_t result = write(vdi->fd, buffer, toWrite);  // Write data
        if (result <= 0) break;

        bytesWritten += result;
        buffer += result;
        count -= result;
        vdi->cursor += result;
    }
    return bytesWritten;
}

// --- Change the logical position inside the VDI (similar to fseek) ---
off_t vdiSeek(VDIFile *vdi, off_t offset, int anchor) {
    if (!vdi) return -1;

    off_t newCursor;
    if (anchor == SEEK_SET) {
        newCursor = offset;            // Absolute seek
    } else if (anchor == SEEK_CUR) {
        newCursor = vdi->cursor + offset; // Relative seek
    } else if (anchor == SEEK_END) {
        newCursor = 134217728 + offset;   // Seek from end (assumed 128MB disk size)
    } else {
        return -1;
    }

    if (newCursor < 0 || newCursor > 134217728) return -1; // Check valid range

    vdi->cursor = newCursor;
    return vdi->cursor;
}

// --- Translate logical offset into physical offset inside the file ---
off_t vdiTranslate(VDIFile *vdi, off_t logicalOffset) {
    uint32_t pageNum = logicalOffset / vdi->pageSize;  // Which page we're in
    uint32_t offsetInPage = logicalOffset % vdi->pageSize; // Offset inside that page

    if (pageNum >= vdi->totalPages) return -1;    // Out of range

    uint32_t physicalPage = vdi->map[pageNum];     // Get mapped physical page number

    if (physicalPage == 0xFFFFFFFF) {
        return -1;  // Page is not allocated
    }

    return (off_t)(physicalPage * vdi->pageSize + offsetInPage); // Calculate physical file offset
}

// --- Print basic header info (signature, version, etc.) ---
void displayVDIHeader(VDIFile *vdi) {
    printf("      Image name: [<<< Oracle VM VirtualBox Disk Image >>>        ]\n");
    printf("       Signature: 0x%x\n", *(uint32_t *)(vdi->header));
    printf("         Version: %d.%d\n", vdi->header[4], vdi->header[5]);
    printf("     Header size: 0x%08x  %d\n", *(uint32_t *)(vdi->header + 12), *(uint32_t *)(vdi->header + 12));
    printf("      Image type: 0x%08x\n", *(uint32_t *)(vdi->header + 16));
    printf("           Flags: 0x%08x\n", *(uint32_t *)(vdi->header + 20));
    printf("     Virtual CHS: %d-%d-%d\n", *(uint16_t *)(vdi->header + 24), *(uint16_t *)(vdi->header + 26), *(uint16_t *)(vdi->header + 28));
    printf("     Sector size: 0x%08x  %d\n", *(uint32_t *)(vdi->header + 32), *(uint32_t *)(vdi->header + 32));
    printf("     Logical CHS: %d-%d-%d\n", *(uint16_t *)(vdi->header + 36), *(uint16_t *)(vdi->header + 38), *(uint16_t *)(vdi->header + 40));
    printf("      Map offset: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 44), *(uint32_t *)(vdi->header + 44));
    printf("    Frame offset: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 48), *(uint32_t *)(vdi->header + 48));
    printf("      Frame size: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 52), *(uint32_t *)(vdi->header + 52));
    printf("Extra frame size: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 56), *(uint32_t *)(vdi->header + 56));
    printf("    Total frames: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 60), *(uint32_t *)(vdi->header + 60));
    printf("Frames allocated: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 64), *(uint32_t *)(vdi->header + 64));
    printf("       Disk size: 0x%016llx  %llu\n", *(uint64_t *)(vdi->header + 68), *(uint64_t *)(vdi->header + 68));

    // Print UUIDs
    printf("            UUID: ...\n");
    printf("  Last snap UUID: ...\n");
    printf("       Link UUID: ...\n");
    printf("     Parent UUID: ...\n");

    // Print full header as a buffer for debugging
    displayBuffer(vdi->header, 400, 0);
}

// --- Read the VDI translation map and display it ---
void displayVDITranslationMap(VDIFile *vdi) {
    uint8_t mbr[512];
    vdiSeek(vdi, 0, SEEK_SET);
    vdiRead(vdi, mbr, 512);
    displayBuffer(mbr, 512, 0x0);
}

// --- Read and display MBR sector (first 512 bytes) ---
void displayMBR(VDIFile *vdi) {
    uint8_t mbr[256];
    vdiSeek(vdi, 0, SEEK_SET);
    vdiRead(vdi, mbr, sizeof(mbr));
    displayBuffer(mbr, sizeof(mbr), 0);
}