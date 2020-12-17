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

// représente une entrée dans la table des pages.
// l'index est le numéro de la page,
// le frame représente le numéro du frame
// active représente si la page est actuellement active dans la mémoire principale
struct page_t {
    int index;
    int frame;
    int active;
};

// Représente une entrée dans la table tlb
// page est un pointeur vers la page en question
// timestamp est un indicateur du temps quand l'entrée fut utilisé
struct tlb_line_t {
    page_t* page;
    clock_t timestamp;
};

// Représente une sortie système pour l'affichage de la mémoire
struct lookup_t {
    int virtualAddress{};
    int physicalAddress{};
    int value{};
    bool found = false;
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

// Permet de verifier si une page est présente dans la table des pages.
lookup_t pageTableLookup(int pageNumber, int offset) {

    for (auto & page : pageTable) {
        if (page.index == pageNumber) { // Si nous avons trouvé la page dans le tableau de pages

            int logicalAddress = (page.index << 8) + offset; // Assembler l'adresse logique avec le numéro de page et l'offset.
            int physicalAddress = (page.frame << 8) + offset;

            // Retourner les informations nécessaire a la journalisation des sorties ainsi que de confirmer que la page fut trouvé
            return lookup_t { logicalAddress,physicalAddress,(int)mainMem[(page.frame * FRAME_SIZE) + offset],true};
        }
    }

    return {};
}

// Permet d'aller cherger, depuis la mémoire secondaire, la page a partir de son numéro.
// l'index de frame permet de savoir ou stocker cette page dans la mémoire principale
void fetchFromDisk(int page, int frameIndex) {

    char buf[PAGE_SIZE] = {0}; // Définition d'un buffer permettant de tenir la pagne avant de la stocker

    disk.seekg(page * PAGE_SIZE); // Se positionner a la position du début de la page
    disk.read(&buf[0], PAGE_SIZE); // Lire a partir de cette position les prochain 256 bytes et les stocker dans le buffer

    // Copier le buffer dans la mémoire principale. Placer la destination du début de la location du frame désiré pour le stockage.
    memcpy(mainMem + (frameIndex * (FRAME_SIZE)), buf,  FRAME_SIZE);
}

// Faire la vérification de l'existence d'une addresse dans la tlb.
lookup_t tlbLookup(int page, int offset) {

    for (int i = 0; i < tlbIndex; i++) {
        if (tlb[i].page->index == page) {

            tlb[i].timestamp = clock(); // Mettre a jour le timestamp de la tlb (car la valeur vient d'être utilisé une nouvelle fois)

            int logicalAddress = (tlb[i].page->index << 8) + offset;
            int physicalAddress = (tlb[i].page->frame << 8) + offset;

            return lookup_t { logicalAddress,physicalAddress,(int)mainMem[(tlb[i].page->frame * FRAME_SIZE) + offset],true};
        }
    }

    return {};
}

// Changer une entrée de page dans tlb
void loadPageToTbl(int pageIndex) {

    int pagePosition = 0;

    for (int i = 0; i < PAGE_ENTRY_AMOUNT; i++) {
        if (pageTable[i].index == pageIndex) {
            pagePosition = i;
        }
    }

    // S'il y a moins de 16 éléments dans la tlb, remplir la tlb.
    if (tlbIndex < 16) {

        // Créer une nouvelle entrée dans la tlb
        tlb[tlbIndex] = tlb_line_t{&pageTable[pagePosition], clock()};

        tlbIndex++;

        return;
    }

    int lruLine = 0;

    // Ceci est l'algorithme du Least Recently Used. Va changer l'entrée la plus ancienne au niveau de son utilisation
    // par la nouvelle page a charger
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
    std::ofstream outputFile("output.txt");

    int frameIndex = 0;

    int address = 0;
    while(addressesFile >> address) {

        addrAmount++;

         int virtualAddressPage = getPage(address);
         int addressOffset = getOffset(address);

         lookup_t result;

         if (!(result = tlbLookup(virtualAddressPage, addressOffset)).found){ // Si l'adresse ne se trouve pas dans la tlb
             tlbMiss++;
             if (!(result = pageTableLookup(virtualAddressPage, addressOffset)).found) { // Si l'adresse ne se trouve pas dans la table de pages

                 fetchFromDisk(virtualAddressPage, frameIndex); // Aller chercher la page correspondant a l'Adresse directement dans la mémoire secondaire

                 pageTable[pageTableIndex] = page_t{virtualAddressPage, frameIndex, 1}; // Ajouter une nouvelle entrée dans la table des pages pour la nouvelle addresse
                 loadPageToTbl(virtualAddressPage); // Ajouter une nouvelle entrée dans la tlb pour L'adresse
                 pageTableIndex++;

                 result = pageTableLookup(virtualAddressPage, addressOffset); // Aller chercher lookup_t pour l'affichage de la sortie.

                 faultAmount++;
                 frameIndex++;

             }
             else {
                 loadPageToTbl(virtualAddressPage); // Si la page n'est pas dans tlb, mais présente dans la table de pages, la charger dans la tlb
             }

         }
         else {
             tlbSuccess++;
         }

        outputFile << "Virtual address: " << result.virtualAddress << " Physical address: " << result.physicalAddress << " Value: " << result.value << std::endl;
    }

    outputFile << "Fault Amount : " << faultAmount << ", Addr Amount: " << addrAmount << std::endl;

    outputFile << "Fault ratio: " << faultAmount / (float)addrAmount << std::endl;

    outputFile << "TLB misses: " << tlbMiss << ", TLB Successes: " << tlbSuccess << ", Rate: " << tlbSuccess / (float) tlbMiss << std::endl;

    outputFile.close();

    return 0;
}
