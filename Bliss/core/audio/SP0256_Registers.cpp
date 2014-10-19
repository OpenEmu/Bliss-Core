
#include "SP0256.h"
#include "SP0256_Registers.h"

SP0256_Registers::SP0256_Registers()
: RAM(2, 0x0080, 0xFFFF, 0xFFFF)
{}

void SP0256_Registers::init(SP0256* ms)
{
    this->ms = ms;
}

void SP0256_Registers::poke(UINT16 location, UINT16 value)
{
    switch(location) {
        //a poke of any value into $80 means that the SP0256 should
        //start speaking
        case 0x0080:
            if (ms->lrqHigh) {
                ms->lrqHigh = FALSE;

                ms->command = value & 0xFF;

                if (!ms->speaking)
                    ms->idle = FALSE;
            }
            break;
        //$81 will reset the SP0256 or push an 8-bit value into the queue
        case 0x0081:
            if (value & 0x0400) {
                ms->resetProcessor();
            }
            else if (ms->fifoSize < SP0256::FIFO_MAX_SIZE) {
                ms->fifoBytes[(ms->fifoHead+ms->fifoSize) & 0x3F] = value;
                ms->fifoSize++;
            }
            break;
    }
}

UINT16 SP0256_Registers::peek(UINT16 location) {
    switch(location) {
        case 0x0080:
            return (ms->lrqHigh ? 0x8000 : 0);
        case 0x0081:
        default:
            return (ms->fifoSize == SP0256::FIFO_MAX_SIZE ? 0x8000 : 0);
    }
}

