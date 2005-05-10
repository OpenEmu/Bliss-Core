
#include "RAM.h"

RAM::RAM(UINT16 size, UINT16 location)
: enabled(TRUE)
{
    this->size = size;
    this->location = location;
    this->readAddressMask = 0xFFFF;
    this->writeAddressMask = 0xFFFF;
    this->bitWidth = sizeof(UINT16)<<3;
    this->trimmer = (UINT16)((1 << (sizeof(UINT16) << 3)) - 1);
    image = new UINT16[size];
}

RAM::RAM(UINT16 size, UINT16 location, UINT8 bitWidth)
: enabled(TRUE)
{
    this->size = size;
    this->location = location;
    this->readAddressMask = 0xFFFF;
    this->writeAddressMask = 0xFFFF;
    this->bitWidth = bitWidth;
    this->trimmer = (UINT16)((1 << bitWidth) - 1);
    image = new UINT16[size];
}

RAM::RAM(UINT16 size, UINT16 location, UINT16 readAddressMask, UINT16 writeAddressMask)
: enabled(TRUE)
{
    this->size = size;
    this->location = location;
    this->readAddressMask = readAddressMask;
    this->writeAddressMask = writeAddressMask;
    this->bitWidth = sizeof(UINT16)<<3;
    this->trimmer = (UINT16)((1 << bitWidth) - 1);
    image = new UINT16[size];
}

RAM::RAM(UINT16 size, UINT16 location, UINT16 readAddressMask, UINT16 writeAddressMask, UINT8 bitWidth)
: enabled(TRUE)
{
    this->size = size;
    this->location = location;
    this->readAddressMask = readAddressMask;
    this->writeAddressMask = writeAddressMask;
    this->bitWidth = bitWidth;
    this->trimmer = (UINT16)((1 << bitWidth) - 1);
    image = new UINT16[size];
}

RAM::~RAM()
{
    delete[] image;
}

void RAM::reset()
{
    enabled = TRUE;
    for (UINT16 i = 0; i < size; i++)
        image[i] = 0;
}

void RAM::SetEnabled(BOOL b)
{
    enabled = b;
}

UINT8 RAM::getBitWidth()
{
    return bitWidth;
}

UINT16 RAM::getReadSize()
{
    return size;
}

UINT16 RAM::getReadAddress()
{
    return location;
}

UINT16 RAM::getReadAddressMask()
{
    return readAddressMask;
}

UINT16 RAM::getWriteSize()
{
    return size;
}

UINT16 RAM::getWriteAddress()
{
    return location;
}

UINT16 RAM::getWriteAddressMask()
{
    return writeAddressMask;
}

UINT16 RAM::peek(UINT16 location)
{
    if (enabled)
        return image[location-this->location];
    else
        return 0xFFFF;
}

void RAM::poke(UINT16 location, UINT16 value)
{
    if (enabled)
        image[location-this->location] = (value & trimmer);
}

