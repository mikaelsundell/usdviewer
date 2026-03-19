// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QTreeWidget>

namespace usdviewer {

class TreeWidgetPrivate;

/**
 * @class TreeWidget
 * @brief Base tree widget for hierarchical data.
 *
 * Provides a customized QTreeWidget with full control over row rendering.
 *
 * - drawRow() handles full-width backgrounds (selection, alternating rows).
 * - Delegates are responsible for item content (icons, text, checkboxes).
 */
class TreeWidget : public QTreeWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs the stage tree widget.
     *
     * @param parent Optional parent widget.
     */
    TreeWidget(QWidget* parent = nullptr);

    /**
     * @brief Destroys the tree widget instance.
     */
    virtual ~TreeWidget();

protected:
    bool viewportEvent(QEvent* event) override;
    
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;

    /**
     * @brief Draws a single row with custom full-width background.
     *
     * Handles row-level rendering such as selection and alternating colors
     * before delegating item content drawing to the base implementation.
     */
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    QScopedPointer<TreeWidgetPrivate> p;
};

}  // namespace usdviewer
