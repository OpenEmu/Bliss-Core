
#ifndef BACKTABRAM_H
#define BACKTABRAM_H

#include "core/memory/RAM.h"

#define BACKTAB_SIZE     0xF0
#define BACKTAB_LOCATION 0x200

class BackTabRAM : public RAM
{

    public:
        BackTabRAM();
        void reset();

        UINT16 peek(UINT16 location);
        void poke(UINT16 location, UINT16 value);

        BOOL areColorAdvanceBitsDirty();
        BOOL isDirty();
        BOOL isDirty(UINT16 location);
        void markClean();

    private:
        UINT16       image[BACKTAB_SIZE];
        BOOL         dirtyBytes[BACKTAB_SIZE];
        BOOL         dirtyRAM;
        BOOL         colorAdvanceBitsDirty;

};

#endif
