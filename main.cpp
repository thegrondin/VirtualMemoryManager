#include <iostream>
#include <fstream>
#include <bitset>
#include <cstring>
#include <sstream>

const int PAGE_ENTRY_AMOUNT = 256;
const int PAGE_SIZE = 256;
const int TBL_ENTRY_AMOUNT = 16;
const int FRAME_SIZE = 256;
const int FRAME_AMOUNT= 256;
const int MEM_SIZE = 65536;

struct page_t {
    int index;
    int frame;
    int active;
};

struct tlb_line_t {
    page_t* page;
    clock_t timestamp;
};

struct lookup_t {
    int virtualAddress{};
    int physicalAddress{};
    int value{};
    bool founded = false;
};


std::ifstream disk("/home/etudiant/CLionProjects/VirtualMemory/simuleDisque.bin", std::ifstream::binary);
char mainMem[MEM_SIZE] = {0};
page_t pageTable[PAGE_ENTRY_AMOUNT] = {0};
tlb_line_t tlb[TBL_ENTRY_AMOUNT];
int tlbIndex = 0;

int pageTableIndex = 0;

int getPage(int addr) {
    return addr >> 8;
}

int getOffset( int addr) {
    int mask = 0b0000000011111111;

    return addr & mask;
}

lookup_t pageTableLookup(int pageNumber, int offset) {

    for (auto & i : pageTable) {
        if (i.index == pageNumber) {

            page_t page = i;

            std::stringstream combinePhysicalStream;
            combinePhysicalStream << page.frame << offset;

            std::stringstream combineLogicalStream;
            combineLogicalStream << page.index << offset;

            int logicalAddress = (page.index << 8) + offset;
            int physicalAddress = (page.frame << 8) + offset;

            return lookup_t { logicalAddress,physicalAddress,(int)mainMem[(page.frame * FRAME_SIZE) + offset],true};
        }
    }

    return {};
}

void fetchFromDisk(int page, int frameIndex) {

    char buf[PAGE_SIZE] = {0};

    disk.seekg(page * PAGE_SIZE);
    disk.read(&buf[0], PAGE_SIZE);

    memcpy(mainMem + (frameIndex * (FRAME_SIZE)), buf,  FRAME_SIZE);
}

lookup_t tlbLookup(int page, int offset) {

    for (int i = 0; i < tlbIndex; i++) {
        if (tlb[i].page->index == page) {

            tlb[i].timestamp = clock();

            int logicalAddress = (tlb[i].page->index << 8) + offset;
            int physicalAddress = (tlb[i].page->frame << 8) + offset;

            return lookup_t { logicalAddress,physicalAddress,(int)mainMem[(tlb[i].page->frame * FRAME_SIZE) + offset],true};
        }
    }

    return {};
}

void loadPageToTbl(int pageIndex) {

    int pagePosition = 0;

    for (int i = 0; i < PAGE_ENTRY_AMOUNT; i++) {
        if (pageTable[i].index == pageIndex) {
            pagePosition = i;
        }
    }


    if (tlbIndex < 16) {

        tlb[tlbIndex] = tlb_line_t{&pageTable[pagePosition], clock()};

        tlbIndex++;

        return;
    }

    int lruLine = 0;

    for (int i = 0; i < TBL_ENTRY_AMOUNT; i++) {
        if (tlb[i].timestamp < tlb[lruLine].timestamp) {
            lruLine = i;
        }
    }

    tlb[lruLine] = tlb_line_t{&pageTable[pagePosition], clock()};

}


int main() {

    int faultAmount = 0;
    int tlbMiss = 0;
    int tlbSuccess = 0;
    int addrAmount = 0;

    std::ifstream addressesFile("adresses.txt");

    int frameIndex = 0;

    int address = 0;
    while(addressesFile >> address) {

        addrAmount++;

         int virtualAddressPage = getPage(address);
         int addressOffset = getOffset(address);

         lookup_t result;

         if (!(result = tlbLookup(virtualAddressPage, addressOffset)).founded){
             tlbMiss++;
             if (!(result = pageTableLookup(virtualAddressPage, addressOffset)).founded) {

                 fetchFromDisk(virtualAddressPage, frameIndex);

                 pageTable[pageTableIndex] = page_t{virtualAddressPage, frameIndex, 1};
                 loadPageToTbl(virtualAddressPage);
                 pageTableIndex++;

                 result = pageTableLookup(virtualAddressPage, addressOffset);

                 faultAmount++;
                 frameIndex++;

             }
             else {
                 loadPageToTbl(virtualAddressPage);
             }

         }
         else {
             tlbSuccess++;
         }

         std::cout << "Virtual address: " << result.virtualAddress << " Physical address: " << result.physicalAddress << " Value: " << result.value << std::endl;
    }

    std::cout << "Fault Amount : " << faultAmount << ", Addr Amount: " << addrAmount << std::endl;

    std::cout << "Fault ratio: " << faultAmount / (float)addrAmount << std::endl;

    std::cout << "TLB misses: " << tlbMiss << ", TLB Successes: " << tlbSuccess << std::endl;

    return 0;
}
