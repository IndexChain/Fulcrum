#pragma once

#include "BTC.h" // for BTC::QByteArrayHashHasher

#include "bitcoin/amount.h"  // for bitcoin::Amount

#include <QByteArray>

#include <functional>

using HashHasher = BTC::QByteArrayHashHasher;

using TxNum = std::uint64_t;
using BlockHeight = std::uint32_t;
using IONum = std::uint16_t;
using TxHash = QByteArray;
using HashX = QByteArray;

using TxHash2NumResolver = std::function< std::optional<TxNum>(const TxHash &) >;
using Num2TxHashResolver = std::function< std::optional<TxHash>(TxNum) >;