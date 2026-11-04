/**
 * @file can_signal.c
 * @brief Couche CAN générique (voir can_signal.h).
 *
 * Les signaux sont parcourus octet par octet (au plus 5 octets pour 32 bits) avec un
 * accumulateur 32 bits : pas d'arithmétique 64 bits, coûteuse sur dsPIC.
 */
#include <string.h>
#include "can_signal.h"
#include "hal/can_port.h"

#define CAN_DATA_SIZE 8u

static uint32_t mask32(uint8_t length)
{
    return (length >= 32u) ? 0xFFFFFFFFUL : (((uint32_t)1 << length) - 1u);
}

static uint8_t mask8(uint8_t n)
{
    return (uint8_t)((1u << n) - 1u);
}

uint32_t CanSignal_GetRaw(const uint8_t *data, const sCanSignalLayout *s)
{
    uint32_t raw = 0;
    uint8_t pos = s->start_bit;
    uint8_t remaining = s->length;
    uint8_t done = 0;
    uint8_t byte, bit, n;

    if ((s->flags & CAN_MOTOROLA) == 0u)
    {
        /* Intel : du bit de poids faible vers les octets suivants */
        while (remaining > 0u)
        {
            byte = pos >> 3;
            bit = pos & 7u;
            n = (uint8_t)(8u - bit);
            if (n > remaining)
                n = remaining;
            raw |= (uint32_t)((data[byte] >> bit) & mask8(n)) << done;
            done += n;
            remaining -= n;
            pos += n;
        }
    }
    else
    {
        /* Motorola : du bit de poids fort, bits décroissants dans l'octet puis octet suivant */
        while (remaining > 0u)
        {
            byte = pos >> 3;
            bit = pos & 7u;
            n = (uint8_t)(bit + 1u);
            if (n > remaining)
                n = remaining;
            raw = (raw << n) | ((data[byte] >> (bit + 1u - n)) & mask8(n));
            remaining -= n;
            pos = (uint8_t)((byte + 1u) * 8u + 7u);
        }
    }
    return raw;
}

int32_t CanSignal_GetInt(const uint8_t *data, const sCanSignalLayout *s)
{
    uint32_t raw = CanSignal_GetRaw(data, s);

    if ((s->flags & CAN_SIGNED) && (s->length < 32u) && (raw & ((uint32_t)1 << (s->length - 1u))))
        raw |= ~mask32(s->length);
    return (int32_t)raw;
}

float CanSignal_GetFloat(const uint8_t *data, const sCanSignalLayout *s)
{
    union { uint32_t u; float f; } v;
    v.u = CanSignal_GetRaw(data, s);
    return v.f;
}

void CanSignal_SetRaw(uint8_t *data, const sCanSignalLayout *s, uint32_t raw)
{
    uint8_t pos = s->start_bit;
    uint8_t remaining = s->length;
    uint8_t done = 0;
    uint8_t byte, bit, n, m;

    raw &= mask32(s->length);

    if ((s->flags & CAN_MOTOROLA) == 0u)
    {
        while (remaining > 0u)
        {
            byte = pos >> 3;
            bit = pos & 7u;
            n = (uint8_t)(8u - bit);
            if (n > remaining)
                n = remaining;
            m = (uint8_t)(mask8(n) << bit);
            data[byte] = (uint8_t)((data[byte] & (uint8_t)~m) | ((uint8_t)((raw >> done) & mask8(n)) << bit));
            done += n;
            remaining -= n;
            pos += n;
        }
    }
    else
    {
        while (remaining > 0u)
        {
            byte = pos >> 3;
            bit = pos & 7u;
            n = (uint8_t)(bit + 1u);
            if (n > remaining)
                n = remaining;
            m = (uint8_t)(mask8(n) << (bit + 1u - n));
            data[byte] = (uint8_t)((data[byte] & (uint8_t)~m) |
                                   ((uint8_t)((raw >> (remaining - n)) & mask8(n)) << (bit + 1u - n)));
            remaining -= n;
            pos = (uint8_t)((byte + 1u) * 8u + 7u);
        }
    }
}

void CanSignal_SetFloat(uint8_t *data, const sCanSignalLayout *s, float value)
{
    union { uint32_t u; float f; } v;
    v.f = value;
    CanSignal_SetRaw(data, s, v.u);
}

/* ---- Liaisons ------------------------------------------------------------------------------ */

static int32_t read_var(const sCanBinding *b)
{
    switch (b->var_type)
    {
    case CAN_VAR_BOOL: return *(const bool *)b->var ? 1 : 0;
    case CAN_VAR_U8:   return *(const uint8_t *)b->var;
    case CAN_VAR_I8:   return *(const int8_t *)b->var;
    case CAN_VAR_U16:  return *(const uint16_t *)b->var;
    case CAN_VAR_I16:  return *(const int16_t *)b->var;
    case CAN_VAR_U32:  return (int32_t)*(const uint32_t *)b->var;
    case CAN_VAR_I32:  return *(const int32_t *)b->var;
    default:           return 0;
    }
}

static void write_var(const sCanBinding *b, int32_t v)
{
    switch (b->var_type)
    {
    case CAN_VAR_BOOL: *(bool *)b->var = (v != 0); break;
    case CAN_VAR_U8:   *(uint8_t *)b->var = (uint8_t)v; break;
    case CAN_VAR_I8:   *(int8_t *)b->var = (int8_t)v; break;
    case CAN_VAR_U16:  *(uint16_t *)b->var = (uint16_t)v; break;
    case CAN_VAR_I16:  *(int16_t *)b->var = (int16_t)v; break;
    case CAN_VAR_U32:  *(uint32_t *)b->var = (uint32_t)v; break;
    case CAN_VAR_I32:  *(int32_t *)b->var = v; break;
    default: break;
    }
}

void CanMsg_Pack(const sCanMessage *m, uint8_t *data)
{
    uint8_t i;
    const sCanBinding *b;
    int32_t v;

    memset(data, 0, CAN_DATA_SIZE);
    for (i = 0; i < m->nb_bindings; i++)
    {
        b = &m->bindings[i];
        if (b->var_type == CAN_VAR_NONE)
            continue;
        if (b->var_type == CAN_VAR_CONST)
        {
            CanSignal_SetRaw(data, &b->layout, (uint32_t)(int32_t)b->offset);
            continue;
        }
        v = read_var(b) - b->offset;
        if (b->layout.flags & CAN_FLOAT32)
            CanSignal_SetFloat(data, &b->layout, (float)v * b->div / b->mul);
        else if ((b->mul == 1) && (b->div == 1))
            CanSignal_SetRaw(data, &b->layout, (uint32_t)v);
        else
            CanSignal_SetRaw(data, &b->layout, (uint32_t)(v * b->div / b->mul));
    }
}

void CanMsg_Unpack(const sCanMessage *m, const uint8_t *data)
{
    uint8_t i;
    const sCanBinding *b;
    int32_t v;

    for (i = 0; i < m->nb_bindings; i++)
    {
        b = &m->bindings[i];
        if (b->var == NULL)
            continue;
        if (b->layout.flags & CAN_FLOAT32)
            v = (int32_t)(CanSignal_GetFloat(data, &b->layout) * b->mul / b->div); /* troncature vers 0 */
        else
        {
            v = CanSignal_GetInt(data, &b->layout);
            if ((b->mul != 1) || (b->div != 1))
                v = v * b->mul / b->div;
        }
        write_var(b, v + b->offset);
    }
}

const sCanMessage *CanMsg_Find(const sCanMessage *table, uint8_t count, uint32_t id)
{
    uint8_t i;
    for (i = 0; i < count; i++)
        if (table[i].id == id)
            return &table[i];
    return NULL;
}

void CanMsg_Send(const sCanMessage *m, uint8_t dlc, CAN_TX_PRIOIRTY priority)
{
    static uint8_t tx_data[CAN_DATA_SIZE];
    static CAN_MSG_OBJ tx_msg;

    CanMsg_Pack(m, tx_data);
    tx_msg.msgId = m->id;
    tx_msg.field.msgfields = 0;
    tx_msg.field.idType = m->extended ? CAN_FRAME_EXT : CAN_FRAME_STD;
    tx_msg.field.frameType = CAN_FRAME_DATA;
    tx_msg.field.dlc = dlc;
    tx_msg.data = tx_data;
    CanPort_Transmit(priority, &tx_msg);
}
