#include "ui/new_brush_preset_dialog.hpp"

#include "ui/dialog_utils.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace patchy::ui {

namespace {
QString dialog_tr(const char* source) {
  return QCoreApplication::translate("patchy::ui::NewBrushPresetDialog", source);
}
}  // namespace

std::optional<NewBrushPresetRequest> request_new_brush_preset(QWidget* parent, const QString& default_name,
                                                             const QStringList& folders,
                                                             const QString& default_folder) {
  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("newBrushPresetDialog"));
  dialog.setWindowTitle(dialog_tr(QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog", "New Brush Preset")));
  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  auto* name_edit = new QLineEdit(default_name, &dialog);
  name_edit->setObjectName(QStringLiteral("newBrushPresetNameEdit"));
  name_edit->selectAll();
  form->addRow(dialog_tr(QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog", "Name:")), name_edit);
  auto* folder_combo = new QComboBox(&dialog);
  folder_combo->setObjectName(QStringLiteral("newBrushPresetFolderCombo"));
  folder_combo->setEditable(true);
  folder_combo->setInsertPolicy(QComboBox::NoInsert);
  folder_combo->addItem(QString());
  folder_combo->addItems(folders);
  folder_combo->setCurrentText(default_folder);
  folder_combo->lineEdit()->setPlaceholderText(
      dialog_tr(QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog", "No folder")));
  folder_combo->setToolTip(dialog_tr(
      QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog", "Pick a folder or type a new folder name")));
  form->addRow(dialog_tr(QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog", "Folder:")), folder_combo);
  layout->addLayout(form);

  const auto add_check = [&dialog, layout](const char* object_name, const char* text, const char* tooltip,
                                           bool checked) {
    auto* check = new QCheckBox(dialog_tr(text), &dialog);
    check->setObjectName(QLatin1String(object_name));
    check->setToolTip(dialog_tr(tooltip));
    check->setChecked(checked);
    layout->addWidget(check);
    return check;
  };
  auto* size_check = add_check(
      "newBrushPresetCaptureSizeCheck", QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog", "Capture brush size in preset"),
      QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog",
                        "Off: picking the preset keeps whatever size you are painting with"),
      true);
  auto* tool_check = add_check(
      "newBrushPresetToolSettingsCheck", QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog", "Include tool settings"),
      QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog",
                        "Opacity, Flow, Smoothing, pen pressure mapping and Mixer Brush values"),
      true);
  auto* color_check = add_check(
      "newBrushPresetColorCheck", QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog", "Include color"),
      QT_TRANSLATE_NOOP("patchy::ui::NewBrushPresetDialog",
                        "Picking the preset also sets the foreground and background colors"),
      false);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  auto* ok = buttons->button(QDialogButtonBox::Ok);
  const auto refresh_ok = [ok, name_edit] { ok->setEnabled(!name_edit->text().trimmed().isEmpty()); };
  QObject::connect(name_edit, &QLineEdit::textChanged, &dialog, refresh_ok);
  refresh_ok();
  dialog.resize(360, dialog.sizeHint().height());
  if (exec_dialog(dialog) != QDialog::Accepted || name_edit->text().trimmed().isEmpty()) {
    return std::nullopt;
  }
  NewBrushPresetRequest request;
  request.name = name_edit->text().trimmed();
  request.options.folder = folder_combo->currentText().trimmed();
  request.options.capture_size = size_check->isChecked();
  request.options.include_tool_settings = tool_check->isChecked();
  request.options.include_color = color_check->isChecked();
  return request;
}

}  // namespace patchy::ui
