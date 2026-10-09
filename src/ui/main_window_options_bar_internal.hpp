#pragma once

// Build-time context shared by MainWindow::build_options_bar()
// (main_window_actions_options_bar.cpp) and the per-tool row builders it calls
// (main_window_options_bar_<theme>.cpp). Never include this header outside
// those TUs.
//
// OptionsBarBuildContext carries what the historical single build_options_bar()
// body kept in function-scope locals: the toolbar and its FlowLayout host, the
// shared row helpers, and the few values one row hands to a later one. It lives
// on build_options_bar()'s stack and dies when it returns, so no lambda may
// capture the context or a reference into it - capture copies of the values
// instead, exactly like the original locals.

#include "ui/main_window.hpp"

#include <QCheckBox>

#include <functional>
#include <initializer_list>
#include <vector>

class QAction;
class QIcon;
class QLayout;
class QPaintEvent;
class QToolBar;
class QWidget;

namespace patchy::ui {

// The Options bar's check box: the box, check mark and frame are painted from
// theme roles (see paintEvent), so it reads in both color schemes.
class CheckGlyphBox final : public QCheckBox {
public:
  explicit CheckGlyphBox(const QString& text, QWidget* parent = nullptr);

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct MainWindow::OptionsBarBuildContext {
  // The startup-defaults donor canvas (ActionBuildContext::canvas_defaults).
  CanvasWidget* canvas_defaults{nullptr};
  QToolBar* toolbar{nullptr};
  // The wrapping FlowLayout and the widget hosting it; every row control is
  // added to it in construction order.
  QWidget* options_content{nullptr};
  QLayout* options_flow{nullptr};

  // The shared row helpers build_options_bar() makes (see there).
  std::function<void(std::initializer_list<CanvasTool>)> add_option_separator;
  std::function<QAction*(const QIcon&, const char*, std::initializer_list<CanvasTool>)> add_option_action;
  std::function<QWidget*(QWidget*, std::vector<CanvasTool>)> add_option_widget;
  std::function<QWidget*(const char*, std::vector<CanvasTool>)> add_option_label;
  std::function<double()> document_ppi;
  std::function<UnitSpinBox::ContextProvider(bool)> document_axis_context;

  // Written by build_shape_appearance_options(), read by the later shape rows.
  std::vector<CanvasTool> vector_appearance_tools;
  UnitSpinBox::ContextProvider thickness_context;

  // Written by the row builders, exported to ActionBuildContext by
  // build_options_bar() for bind_action_translations().
  QAction* brush_smaller_action{nullptr};
  QAction* brush_larger_action{nullptr};
  QAction* brush_much_smaller_action{nullptr};
  QAction* brush_much_larger_action{nullptr};
  QCheckBox* fill_shapes{nullptr};
};

}  // namespace patchy::ui
