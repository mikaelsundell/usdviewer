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
    void mousePressEvent(QMouseEvent* event) override;

    void mouseReleaseEvent(QMouseEvent* event) override;

    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index,
                                                         const QEvent* event = nullptr) const override;

    /**
     * @brief Handles mouse interaction for branch expand/collapse.
     *
     * Intercepts mouse presses in the branch hit area and toggles
     * expansion manually. Other events are forwarded to the base class.
     *
     * @note Hit area must match drawBranches() positioning.
     */
    bool viewportEvent(QEvent* event) override;

    /**
     * @brief Draws custom branch (expand/collapse) icons.
     *
     * Replaces Qt’s default branch rendering to control icon style
     * and positioning. Uses the provided @p rect as the branch area.
     *
     * @note Do not use visualRect(); it breaks alignment.
     */
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
