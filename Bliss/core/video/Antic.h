
#ifndef ANTIC_H
#define ANTIC_H

#include <d3d9.h>

#include "Antic_Registers.h"
#include "VideoProducer.h"
#include "GTIA.h"
#include "core/cpu/Processor.h"
#include "core/cpu/SignalLine.h"
#include "core/memory/MemoryBus.h"

#define ANTIC_PIN_OUT_NMI   0
#define ANTIC_PIN_OUT_HALT  1
#define ANTIC_PIN_OUT_READY 2

class Antic : public Processor, public VideoProducer
{
    friend class Antic_Registers;

    public:
        Antic();
        Antic(MemoryBus* memoryBus, GTIA* gtia);
        BOOL inVerticalBlank() { return inVBlank; }
        Antic_Registers* getRegisters();
        void resetProcessor();
        INT32 getClockSpeed();
        INT32 tick(INT32);
		void setVideoOutputDevice(IDirect3DDevice9*);

        void getOutputImageSize(UINT16* width, UINT16* height);
        void getPalette(const UINT32** palette, UINT16* numEntries);
        void render();

    private:
        void render_blank();
        void render_2();
        void render_3();
        void render_4();
        void render_5();
        void render_6();
        void render_7();
        void render_8();
        void render_9();
        void render_A();
        void render_B();
        void render_C();
        void render_D();
        void render_E();
        void render_F();

        Antic_Registers registers;
        MemoryBus* memoryBus;
        GTIA* gtia;
        BOOL inVBlank;

        UINT8 INST;
        UINT8 LCOUNT;
        UINT8 HCOUNT;
        UINT16 MEMSCAN;
        UINT8 MODE;
        UINT8 BYTEWIDTH;
        UINT8 BLOCKLENGTH;
        UINT8* imageBank;

        //registers
        UINT8  SHIFT[48];
        UINT8  DMACTL;
        UINT8  CHACTL;
        UINT16 DLIST;
        UINT8  HSCROL;
        UINT8  VSCROL;
        UINT8  PMBASE;
        UINT8  CHBASE;
        UINT16 VCOUNT;
        UINT8  NMIEN;
        UINT8  NMIST;

        const static UINT8 BLOCK_LENGTHS[14];
        const static UINT8 BYTE_WIDTHS[14][3];
        const static UINT32 palette[256];

};

#endif

