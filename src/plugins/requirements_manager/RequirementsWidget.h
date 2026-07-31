#pragma once

#include <QWidget>
#include <QVector>
#include "RequirementsParser.h"

class QTreeView;
class QTextBrowser;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QListWidget;
class QSplitter;

namespace Daqster {

class RequirementsModel;

/**
 * @brief Requirements Viewer/Editor widget.
 *
 * Left: tree of requirements (grouped by active/archive section).
 * Right: details panel with two modes:
 *   - Preview Mode: read-only formatted view + interactive acceptance
 *     criteria checkboxes.
 *   - Edit Mode: raw Markdown editor (QPlainTextEdit) with Save/Cancel.
 *
 * Changes are written back to the .md files (bidirectional sync).
 */
class RequirementsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RequirementsWidget(QWidget *parent = nullptr);
    ~RequirementsWidget() override;

    void openDirectory(const QString &baseDir);

    QString baseDirectory() const;

private slots:
    void onSelectionChanged();
    void onEditToggled(bool edit);
    void onSave();
    void onCancel();
    void onCriterionToggled(int index, bool done);
    void onBrowseDirectory();

private:
    void reload();
    void showPreview();
    void updatePreviewText(const Requirement &req);

    RequirementsModel *m_model;
    QTreeView *m_treeView;
    QTextBrowser *m_preview;
    QPlainTextEdit *m_editor;
    QLabel *m_fileLabel;
    QPushButton *m_editButton;
    QPushButton *m_saveButton;
    QPushButton *m_cancelButton;
    QPushButton *m_browseButton;
    QListWidget *m_criteriaList;
    QSplitter *m_splitter;

    QString m_baseDir;
    QVector<Requirement> m_requirements;
    int m_currentIndex; //!< index into m_requirements; -1 when none selected
};

} // namespace Daqster
