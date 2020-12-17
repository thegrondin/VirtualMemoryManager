#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <cstring>
#include <sstream>
#include <thread>

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
    int virtualAddress;
    int physicalAddress;
    int value;
    bool founded = false;
};


std::ifstream disk("/home/etudiant/CLionProjects/VirtualMemory/simuleDisque.bin", std::ifstream::binary);
char mainMem[MEM_SIZE] = {0};
page_t pageTable[PAGE_ENTRY_AMOUNT] = {0};
tlb_line_t tlb[TBL_ENTRY_AMOUNT];
int tlbIndex = 0;

int pageTableIndex = 0;




// 8 + 8 + 1 + 8
// 39425
// 1001 1010 0000 0001
// page : 1001 1010
int getPage(int addr) {

    auto localVal =  addr >> 8;
    //std::cout << "IN : getPage, page: " << localVal << std::endl;

    return addr >> 8;

}

int getOffset( int addr) {


    int mask = 0b0000000011111111;
    auto localVal =  addr & mask;
   // std::cout << "IN : getOffset, offset: " << localVal << std::endl;


    return addr & mask;

}



int test2Amount = 0;

lookup_t pageTableLookup(int pageNumber, int offset) {



    for (int i = 0; i < PAGE_ENTRY_AMOUNT; i++) {
        if (pageTable[i].index == pageNumber) {
            test2Amount++;
            page_t page = pageTable[i];

            std::stringstream combinePhysicalStream;
            combinePhysicalStream << page.frame << offset;

            std::stringstream combineLogicalStream;
            combineLogicalStream << page.index << offset;

            //std::cout << "Phys: " << combinePhysicalStream.str() << ", Virt: " << combineLogicalStream.str() << ", Val: "<< (int)mainMem[(page.frame * FRAME_SIZE) + offset] << std::endl;
            //std::cout << "IN : pageTableLookup, page.frame: " << page.frame << ", offset: " << offset << ", (page.frame * FRAME_SIZE) + offset: " << (page.frame * FRAME_SIZE) + offset << std::endl;

           // std::cout << (int)mainMem[(page.frame * FRAME_SIZE) + offset] << std::endl;

            int logicalAddress = (page.index << 8) + offset;
            int physicalAddress = (page.frame << 8) + offset;



            return lookup_t { logicalAddress,physicalAddress,(int)mainMem[(page.frame * FRAME_SIZE) + offset],true};




        }
    }
}

void fetchFromDisk(int page, int frameIndex) {

    char buf[PAGE_SIZE] = {0};

    disk.seekg(page * PAGE_SIZE);
    disk.read(&buf[0], PAGE_SIZE);

    //std::cout << "IN: fetchFromDisk, frameIndex : " << frameIndex << ", Frame step: " << (frameIndex * FRAME_SIZE) + FRAME_SIZE << std::endl;
    memcpy(mainMem + (frameIndex * (FRAME_SIZE)), buf,  FRAME_SIZE);

}

lookup_t tlbLookup(int page, int offset) {

    for (int i = 0; i < tlbIndex; i++) {
        if (tlb[i].page->index == page) {

            tlb[i].timestamp = clock();

            // TODO: Resolve repetition

            int logicalAddress = (tlb[i].page->index << 8) + offset;
            int physicalAddress = (tlb[i].page->frame << 8) + offset;



            return lookup_t { logicalAddress,physicalAddress,(int)mainMem[(tlb[i].page->frame * FRAME_SIZE) + offset],true};


        }
    }

    return lookup_t{0,0,0,false};

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
    int testAmount = 0;


    std::ifstream addressesFile("/home/etudiant/CLionProjects/VirtualMemory/adresses.txt");

    int frameIndex = 0;

    int address = 0;
    while(addressesFile >> address) {
        //std::cout << "Address: " << address << std::endl;
        addrAmount++;


         int virtualAddressPage = getPage(address);
         int addressOffset = getOffset(address);

         lookup_t result;

         if (!(result = tlbLookup(virtualAddressPage, addressOffset)).founded){
             tlbMiss++;
             if (!(result = pageTableLookup(virtualAddressPage, addressOffset)).founded) {
                 testAmount++;

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

         //Virtual address: 2301 Physical address: 35325 Value: 0
         std::bitset<16> virt(result.virtualAddress);
         std::bitset<16> phys(result.physicalAddress);
         std::cout << "Virtual address: " << result.virtualAddress << " Physical address: " << result.physicalAddress << " Value: " << result.value << std::endl;

         //std::cout << address << ", Page : " << virtualAddressPage << ", offset: " << addressOffset << std::endl;
    }


    std::cout << "Fault Amount : " << faultAmount << ", Addr Amount: " << addrAmount << ", Test amount: " << test2Amount << std::endl;

    std::cout << "Fault ratio: " << faultAmount / (float)addrAmount << std::endl;

    std::cout << "TLB misses: " << tlbMiss << ", TLB Successes: " << tlbSuccess << std::endl;

    return 0;
}
