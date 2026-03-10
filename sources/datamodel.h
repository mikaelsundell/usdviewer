// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QExplicitlySharedDataPointer>
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QVariant>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {

class DataModelPrivate;

/**
 * @class DataModel
 * @brief Central data model managing the USD stage and scene state.
 *
 * Provides access to the currently loaded USD stage and maintains
 * application state such as progress notifications, scene masks,
 * bounding boxes, and status messages. The model acts as a shared
 * data source for viewer components like the stage tree, render
 * view, and property inspector.
 *
 * DataModel instances use implicit sharing and thread-safe access
 * to the underlying USD stage.
 */
class DataModel : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Stage loading policy.
     */
    enum load_policy {
        load_all,     ///< Load the entire stage.
        load_payload  ///< Load stage with payloads deferred.
    };

    /**
     * @brief Progress block state.
     */
    enum progress_mode {
        progress_idle,    ///< No progress operation running.
        progress_running  ///< Progress operation active.
    };

    /**
     * @brief Stage loading status.
     */
    enum stage_status {
        stage_loaded,  ///< Stage successfully loaded.
        stage_failed,  ///< Stage loading failed.
        stage_closed   ///< Stage has been closed.
    };

public:
    /**
     * @struct Notify
     * @brief Progress notification payload.
     *
     * Used to describe updates emitted during long-running
     * operations such as stage loading or export tasks.
     */
    struct Notify {
        QString message;       ///< Notification message.
        QList<SdfPath> paths;  ///< Associated prim paths.
        QVariantMap details;   ///< Additional metadata.

        Notify() = default;

        Notify(const QString& n, const QList<SdfPath>& p, const QVariantMap& d = {})
            : message(n)
            , paths(p)
            , details(d)
        {}
    };

public:
    /**
     * @brief Constructs an empty data model.
     */
    DataModel();

    /**
     * @brief Constructs and loads a stage from file.
     *
     * @param filename USD file to load.
     * @param policy Stage loading policy.
     */
    DataModel(const QString& filename, load_policy policy = load_all);

    /**
     * @brief Copy constructor.
     */
    DataModel(const DataModel& other);

    /**
     * @brief Destroys the DataModel instance.
     */
    ~DataModel();

    /** @name Progress Reporting */
    ///@{

    /**
     * @brief Begins a progress block.
     *
     * @param name Name of the progress operation.
     * @param count Expected number of steps.
     */
    void beginProgressBlock(const QString& name, size_t count = 0);

    /**
     * @brief Updates progress with a notification.
     *
     * @param notify Notification payload.
     * @param completed Number of completed steps.
     */
    void updateProgressNotify(const Notify& notify, size_t completed);

    /**
     * @brief Cancels the active progress block.
     */
    void cancelProgressBlock();

    /**
     * @brief Ends the current progress block.
     */
    void endProgressBlock();

    /**
     * @brief Returns whether the progress block has been cancelled.
     */
    bool isProgressBlockCancelled() const;

    ///@}

    /** @name Stage Operations */
    ///@{

    /**
     * @brief Loads a USD stage from file.
     *
     * @param filename File to load.
     * @param policy Stage loading policy.
     *
     * @return True if loading succeeded.
     */
    bool loadFromFile(const QString& filename, load_policy policy = load_all);

    /**
     * @brief Saves the current stage to file.
     */
    bool saveToFile(const QString& filename);

    /**
     * @brief Exports the entire stage to a file.
     */
    bool exportToFile(const QString& filename);

    /**
     * @brief Exports specific prim paths to a file.
     */
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);

    /**
     * @brief Reloads the currently opened stage.
     */
    bool reload();

    /**
     * @brief Closes the current stage.
     */
    bool close();

    /**
     * @brief Returns whether a stage is currently loaded.
     */
    bool isLoaded() const;

    ///@}

    /** @name Scene State */
    ///@{

    /**
     * @brief Sets the active prim mask.
     */
    void setMask(const QList<SdfPath>& paths);

    /**
     * @brief Sets a textual status message.
     */
    void setStatus(const QString& status);

    /**
     * @brief Returns the current loading policy.
     */
    load_policy loadPolicy() const;

    /**
     * @brief Returns the scene bounding box.
     */
    GfBBox3d boundingBox();

    /**
     * @brief Returns the current stage filename.
     */
    QString filename() const;

    /**
     * @brief Returns the active USD stage.
     */
    UsdStageRefPtr stage() const;

    /**
     * @brief Returns the stage lock used for thread-safe access.
     */
    QReadWriteLock* stageLock() const;

    ///@}

Q_SIGNALS:

    /**
     * @brief Emitted when a progress block begins or ends.
     */
    void progressBlockChanged(const QString& name, progress_mode mode);

    /**
     * @brief Emitted when progress updates occur.
     */
    void progressNotifyChanged(const Notify& notify, size_t completed, size_t expected);

    /**
     * @brief Emitted when the scene bounding box changes.
     */
    void boundingBoxChanged(const GfBBox3d& bbox);

    /**
     * @brief Emitted when the prim mask changes.
     */
    void maskChanged(const QList<SdfPath>& paths);

    /**
     * @brief Emitted when prims are modified.
     */
    void primsChanged(const QList<SdfPath>& paths);

    /**
     * @brief Emitted when the stage changes.
     */
    void stageChanged(UsdStageRefPtr stage, load_policy policy, stage_status status);

    /**
     * @brief Emitted when the status message changes.
     */
    void statusChanged(const QString& status);

private:
    QExplicitlySharedDataPointer<DataModelPrivate> p;
};

}  // namespace usd
