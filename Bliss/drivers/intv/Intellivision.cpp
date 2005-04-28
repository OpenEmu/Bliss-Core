
#include "Intellivision.h"

/**
 * Initializes all of the basic hardware included in the Intellivision
 * Master Component as well as the ECS and Intellivoice peripherals.
 * This method is called only once when the Intellivision is constructed.
 */
Intellivision::Intellivision()
    : Emulator("Intellivision"),
      player1Controller(0, "Hand Controller #1"),
      player2Controller(1, "Hand Controller #2"),
      psg(0x01F0, &player1Controller, &player2Controller),
      RAM8bit(0x00F0, 0x0100, 8),
      RAM16bit(0x0160, 0x0200, 16),
      execROM("Executive ROM", "exec.bin", 0, 2, 0x1000, 0x1000),
      grom(),
      gram(),
      cpu(&memoryBus, 0x1000, 0x1004),
      stic(&memoryBus, &grom, &gram)
{
    //make the pin connections from the CPU to the STIC
    stic.connectPinOut(AY38900_PIN_OUT_SR1, &cpu, CP1610_PIN_IN_INTRM);
    stic.connectPinOut(AY38900_PIN_OUT_SR2, &cpu, CP1610_PIN_IN_BUSRQ);
    cpu.connectPinOut(CP1610_PIN_OUT_BUSAK, &stic, AY38900_PIN_IN_SST);

    //add the player one hand controller
    AddInputConsumer(&player1Controller);

    //add the player two hand controller
    AddInputConsumer(&player2Controller);

    //add the 8-bit RAM
    AddRAM(&RAM8bit);

    //add the 16-bit RAM
    AddRAM(&RAM16bit);

    //add the executive ROM
    AddROM(&execROM);

    //add the GROM
    AddROM(&grom);

    //add the GRAM
    AddRAM(&gram);

	//add the backtab ram
	AddRAM(&stic.backtab);

    //add the CPU
    AddProcessor(&cpu);

    //add the STIC
    AddProcessor(&stic);
    AddVideoProducer(&stic);

    //add the STIC registers
    AddRAM(&stic.registers);

    //add the PSG
    AddProcessor(&psg);
    AddAudioProducer(&psg);

    //add the PSG registers
    AddRAM(&psg.registers);

    AddPeripheral(&ecs);
    AddPeripheral(&intellivoice);
}
