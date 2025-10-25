#pragma once
#include "tx.h"
#include "block.h"

// -------------- Parsers (inverse of serialize) ------------
OutPoint    parse_outpoint(const uint8_t*& p, const uint8_t* end);
TxIn        parse_txin(const uint8_t*& p, const uint8_t* end);
TxOut       parse_txout(const uint8_t*& p, const uint8_t* end);
Transaction parse_tx(const uint8_t*& p, const uint8_t* end);
BlockHeader parse_header(const uint8_t*& p, const uint8_t* end);
Block       parse_block(const uint8_t*& p, const uint8_t* end);