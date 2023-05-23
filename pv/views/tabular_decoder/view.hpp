/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2020 Soeren Apel <soeren@apelpie.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_VIEWS_TABULAR_DECODER_VIEW_HPP
#define PULSEVIEW_PV_VIEWS_TABULAR_DECODER_VIEW_HPP

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QToolButton>
#include <QListWidget>

#include "pv/metadata_obj.hpp"
#include "pv/views/viewbase.hpp"
#include "pv/data/decodesignal.hpp"

#include "pv/widgets/popuptoolbutton.hpp"
#include "pv/popups/tabularclasses.hpp"

extern uint64_t time();
#define tDebug(t, f, ...) qDebug("%s " f, std::to_string(time() - t).c_str(), __VA_ARGS__)

namespace pv {
class Session;

using widgets::PopupToolButton;
using popups::TabularClasses;

namespace views {

namespace tabular_decoder {

// When adding an entry here, don't forget to update SaveTypeNames as well
enum SaveType {
	SaveTypeCSVEscaped,
	SaveTypeCSVQuoted,
	SaveTypeCount  // Indicates how many save types there are, must always be last
};

// When adding an entry here, don't forget to update ViewModeNames as well
enum ViewModeType {
	ViewModeAll,
	ViewModeLatest,
	ViewModeVisible,
	ViewModeCount // Indicates how many view mode types there are, must always be last
};

// Unique identifier for an annotation class of a particular decoder
union AnnotationClassId {
	struct {
		uint32_t dec_stack_level;
		uint32_t ann_id;
	};
	uint64_t id;
};

extern const char* SaveTypeNames[SaveTypeCount];
extern const char* ViewModeNames[ViewModeCount];


class AnnotationCollectionModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	AnnotationCollectionModel(QObject* parent = nullptr);

	int get_hierarchy_level(const Annotation* ann) const;
	QVariant data_from_ann(const Annotation* ann, int index) const;
	QVariant data(const QModelIndex& index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;

	uint8_t first_hidden_column() const;
	QVariant headerData(int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column,
		const QModelIndex& parent_idx = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex& index) const override;

	int rowCount(const QModelIndex& parent_idx = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent_idx = QModelIndex()) const override;

	void set_signal_and_segment(data::DecodeSignal* signal, uint32_t current_segment);
	void set_visible_classes(const std::unordered_set<decltype(AnnotationClassId::id)> &ann_class_ids);
	void update_data();

	QModelIndex update_highlighted_rows(QModelIndex first, QModelIndex last,
		int64_t sample_num);

	const deque<const Annotation*>* all_annotations_;
private:
	vector<QVariant> header_data_;
	deque<const Annotation*> visible_annotations_;
	data::DecodeSignal* signal_;
	uint8_t first_hidden_column_;
	uint32_t prev_segment_;
	uint64_t prev_last_row_;
	uint64_t prev_last_row_index_;
	int64_t highlight_sample_num_;
	bool had_highlight_before_;
	std::unordered_set<decltype(AnnotationClassId::id)> visible_ann_class_ids_;
	mutex* output_mutex_;

	void update_visible_annotations();
};


class CustomFilterProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT

public:
	CustomFilterProxyModel(QObject* parent = 0);

	void set_sample_range(uint64_t start_sample, uint64_t end_sample);

	void enable_range_filtering(bool value);

protected:
	bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
	uint64_t range_start_sample_, range_end_sample_;
	bool range_filtering_enabled_;
};


class CustomTableView : public QTableView
{
	Q_OBJECT

public:
	virtual QSize sizeHint() const override;

protected:
	virtual void keyPressEvent(QKeyEvent *event) override;

Q_SIGNALS:
	void activatedByKey(const QModelIndex &index);
};


class View : public ViewBase, public MetadataObjObserverInterface
{
	Q_OBJECT

public:
	explicit View(Session &session, bool is_main_view=false, QMainWindow *parent = nullptr);
	~View();

	virtual ViewType get_type() const;

	/**
	 * Resets the view to its default state after construction. It does however
	 * not reset the signal bases or any other connections with the session.
	 */
	virtual void reset_view_state();

	virtual void clear_decode_signals();
	virtual void add_decode_signal(shared_ptr<data::DecodeSignal> signal);
	virtual void remove_decode_signal(shared_ptr<data::DecodeSignal> signal);

	virtual void save_settings(QSettings &settings) const;
	virtual void restore_settings(QSettings &settings);

	virtual QSize sizeHint() const override;
private:
	void reset_data();
	void update_data();
	void update_selectors(const data::DecodeSignal* signal);
	void update_class_visibility();

	void save_data_as_csv(unsigned int save_type) const;

private Q_SLOTS:
	void on_selected_signal_changed(int index);
	void on_selected_classes_changed();
	void on_hide_hidden_changed(bool checked);
	void on_view_mode_changed(int index);

	void on_annotation_visibility_changed();
	void on_signal_name_changed(const QString &name);
	void on_signal_color_changed(const QColor &color);
	void on_new_annotations();

	void on_decoder_reset();
	void on_decoder_stack_changed(void* decoder);

	void on_actionSave_triggered(QAction* action = nullptr);

	void on_table_item_clicked(const QModelIndex& index);
	void on_table_item_double_clicked(const QModelIndex& index);
	void on_table_header_requested(const QPoint& pos);
	void on_table_header_toggled(bool checked);

	virtual void on_metadata_object_changed(MetadataObject* obj,
		MetadataValueType value_type);

	virtual void perform_delayed_view_update();

private:
	QWidget* parent_;

	QToolBar* toolbar_;
	QComboBox* signal_selector_;
	QListWidget* class_selector_;
	PopupToolButton class_selector_button_;
	QCheckBox* hide_hidden_cb_;
	QComboBox* view_mode_selector_;

	QToolButton* save_button_;
	QAction* save_action_;

	CustomTableView* table_view_;
	AnnotationCollectionModel* model_;
	CustomFilterProxyModel* filter_proxy_model_;

	data::DecodeSignal* signal_;
};

} // namespace tabular_decoder
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TABULAR_DECODER_VIEW_HPP
