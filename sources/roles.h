// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <Qt>

namespace usdviewer::roles::shelf {

/**
 * @brief Item data role storing the user-visible script name.
 */
inline constexpr int scriptName = Qt::UserRole + 1;

/**
 * @brief Item data role storing a PNG-encoded custom script icon.
 */
inline constexpr int scriptIcon = Qt::UserRole + 2;

}  // namespace usdviewer::roles::shelf
