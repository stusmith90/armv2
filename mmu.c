#include "armv2.h"
#include <stdio.h>
#include <stdarg.h>

armv2status_t MmuDataOperation(armv2_t *cpu,uint32_t crm, uint32_t aux, uint32_t crd, uint32_t crn, uint32_t opcode) {
    return ARMV2STATUS_OK;
}

armv2status_t MmuRegisterTransfer      (armv2_t *cpu, uint32_t crm, uint32_t aux, uint32_t crd, uint32_t crn, uint32_t opcode) {
    return ARMV2STATUS_OK;
}
