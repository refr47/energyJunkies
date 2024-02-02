#ifdef FRONIUS_IV

#define __MODBUS_REGISTER_CPP

#include "modbusRegister.h"
#include "debugConsole.h"

// swap bytes of an byte array
void swapRegs(uint16_t regs[], int count)
{
    uint16_t temp;
    for (int i = 0; i < count / 2; i++)
    {
        temp = regs[i];
        regs[i] = regs[count - 1 - i];
        regs[count - 1 - i] = temp;
    }
}
 
int scaleValues(double target[], int16_t source[], SCALE_INDEX_t relation[], int count)
{
    int64_t value;

    // DBGf("scaleValues :: BEGIN and count: %d", count);
    for (int i = 0; i < count; i++)
    {
        // DBGf("Index: %d", i);
        switch (relation[i].regCount)
        {
        case 4:
        {
            REG_VALUE64_t valueUnion64;
            valueUnion64.regs[0] = source[relation[i].sourceIndex];
            valueUnion64.regs[1] = source[relation[i].sourceIndex + 1];
            valueUnion64.regs[2] = source[relation[i].sourceIndex + 2];
            valueUnion64.regs[3] = source[relation[i].sourceIndex + 3];
            // swapBytes(valueUnion64.bytes, relation[i].regCount * 2);
            swapRegs(valueUnion64.regs, relation[i].regCount);
            value = valueUnion64.value64;
            // DBGf("scaleValues (4); index: %d value: %d scaleFac: %d", i, value, relation[i].scaleIndex);
            break;
        }
        case 2:
        {
            REG_VALUE32_t valueUnion32;
            valueUnion32.regs[0] = source[relation[i].sourceIndex];
            valueUnion32.regs[1] = source[relation[i].sourceIndex + 1];
            // swapBytes(valueUnion32.bytes, relation[i].regCount * 2);
            swapRegs(valueUnion32.regs, relation[i].regCount);
            value = valueUnion32.value32;
            // value = ((uint32_t)(source[relation[i].sourceIndex + 1])) << 16;
            // value |= (uint16_t)source[relation[i].sourceIndex];
            // value = swapBytes((uint16_t)(source[relation[i].sourceIndex + 1]));
            // value = value << 16 + swapBytes(source[relation[i].sourceIndex]);
            // DBGf("scaleValues (2); index: %d value: %d scaleFac: %d", i, value, relation[i].scaleIndex);
            break;
        }
        case 1:
        default:
            value = source[relation[i].sourceIndex];
            
        }
       
        if (relation[i].scaleIndex < 0)
        {
            target[relation[i].targetIndex] = value;
        }
        else
        {
            target[relation[i].targetIndex] = value * pow(10, source[relation[i].scaleIndex]);
        }
    }
    // DBGf("scaleValues :: END and count: %d", count);
    return 0;
}
u8_t getHighByte(uint16_t b)
{
    return (b >> 8) & 0xff;
}
u8_t getLowByte(uint16_t b)
{
    return b & 0xff;
}
void makeString(int indexF, int indexT, int16_t *regArr, char **stringBase)
{
    char *base = *stringBase;
    for (int jj = indexF; jj < indexT; jj++)
    {
        if (regArr + jj == 0)
            return;
        *base = getHighByte(regArr[jj]);
        if (*base == 0)
            return;
        *(++base) = 0;
        *base = getLowByte(regArr[jj]);
        if (*base == 0)
            return;
        *(++base) = 0;
    }
}

#endif