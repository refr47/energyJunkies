/*
 * ──────────────────────────────────────────────────────
 *  Modbus Register Utilities
 *
 *  scaleValues: kombiniert 16-Bit Holding-Registers zu
 *  16/32/64-Bit Werten, wendet Skalierungsfaktor an.
 * ──────────────────────────────────────────────────────
 */

#include "modbusRegister.h"

static void swapRegs(uint16_t regs[], int count)
{
    for (int i = 0; i < count / 2; i++)
    {
        uint16_t tmp = regs[i];
        regs[i] = regs[count - 1 - i];
        regs[count - 1 - i] = tmp;
    }
}

int scaleValues(double target[], int16_t source[], SCALE_INDEX_t relation[], int count)
{
    int64_t value;

    for (int i = 0; i < count; i++)
    {
        switch (relation[i].regCount)
        {
        case 4:
        {
            REG_VALUE64_t u;
            u.regs[0] = source[relation[i].sourceIndex];
            u.regs[1] = source[relation[i].sourceIndex + 1];
            u.regs[2] = source[relation[i].sourceIndex + 2];
            u.regs[3] = source[relation[i].sourceIndex + 3];
            swapRegs(u.regs, relation[i].regCount);
            value = u.value64;
            break;
        }
        case 2:
        {
            REG_VALUE32_t u;
            u.regs[0] = source[relation[i].sourceIndex];
            u.regs[1] = source[relation[i].sourceIndex + 1];
            swapRegs(u.regs, relation[i].regCount);
            value = u.value32;
            break;
        }
        case 1:
        default:
            value = source[relation[i].sourceIndex];
            break;
        }

        /* Skalierungsfaktor anwenden oder direkt übernehmen */
        if (relation[i].scaleIndex < 0)
        {
            target[relation[i].targetIndex] = (double)value;
        }
        else
        {
            target[relation[i].targetIndex] = (double)value * pow(10, source[relation[i].scaleIndex]);
        }
    }

    return 0;
}
