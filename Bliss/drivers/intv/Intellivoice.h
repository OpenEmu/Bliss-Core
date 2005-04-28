
#ifndef INTELLIVOICE_H
#define INTELLIVOICE_H

#include "Memory.h"
#include "core/Peripheral.h"
#include "core/types.h"
#include "core/cpu/Processor.h"
#include "core/audio/SP0256.h"
#include "core/audio/AudioOutputLine.h"

class Intellivoice : public Peripheral
{

public:
	Intellivoice()
		: Peripheral("Intellivoice", "Intellivoice")
	{
		AddProcessor(&sp0256);
        AddAudioProducer(&sp0256);
        AddRAM(&sp0256.registers);
		AddROM(&sp0256.ivoiceROM);
	}
    UINT32 getProcessorCount();
    void getProcessor(INT32 i, Processor** p);
    UINT32 getMemoryCount();
    void getMemory(INT32 i, Memory** m);

private:
    SP0256         sp0256;

};

#endif
