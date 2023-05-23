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

#include <QApplication>
#include <QDebug>
#include <QString>

#include "pv/views/tabular_decoder/view.hpp"

#include "view.hpp"

#include "pv/util.hpp"
#include "pv/globalsettings.hpp"

using std::make_shared;

using pv::util::Timestamp;
using pv::util::format_time_si;
using pv::util::format_time_minutes;
using pv::util::SIPrefix;

namespace pv {
namespace views {
namespace tabular_decoder {

AnnotationCollectionModel::AnnotationCollectionModel(QObject* parent) :
	QAbstractTableModel(parent),
	all_annotations_(nullptr),
	signal_(nullptr),
	first_hidden_column_(0),
	prev_segment_(0),
	prev_last_row_(0),
	prev_last_row_index_(0),
	had_highlight_before_(false),
	visible_ann_class_ids_(),
	output_mutex_(nullptr)
{
	// Note: when adding entries, consider ViewVisibleFilterProxyModel::filterAcceptsRow()

	uint8_t i = 0;
	header_data_.emplace_back(tr("Sample"));    i++; // Column #0
	header_data_.emplace_back(tr("Time"));      i++; // Column #1
	header_data_.emplace_back(tr("Decoder"));   i++; // Column #2
	header_data_.emplace_back(tr("Ann Row"));   i++; // Column #3
	header_data_.emplace_back(tr("Ann Class")); i++; // Column #4
	header_data_.emplace_back(tr("Value"));     i++; // Column #5

	first_hidden_column_ = i;
	header_data_.emplace_back("End Sample");         // Column #6, hidden
}

int AnnotationCollectionModel::get_hierarchy_level(const Annotation* ann) const
{
	int level = 0;

	const unsigned int ann_stack_level = ann->row_data()->row()->decoder()->get_stack_level();
	level = (signal_->decoder_stack().size() - 1 - ann_stack_level);

	return level;
}

QVariant AnnotationCollectionModel::data_from_ann(const Annotation* ann, int index) const
{
	switch (index) {
	case 0: return QVariant((qulonglong)ann->start_sample());  // Column #0, Start Sample
	case 1: {                                                  // Column #1, Start Time
			Timestamp t = ann->start_sample() / signal_->get_samplerate();
			QString unit = signal_->get_samplerate() ? tr("s") : tr("sa");
			QString s;
			if ((t < 60) || (signal_->get_samplerate() == 0))  // i.e. if unit is sa
				s = format_time_si(t, SIPrefix::unspecified, 3, unit, false);
			else
				s = format_time_minutes(t, 3, false);
			return QVariant(s);
		}
	case 2: return QVariant(ann->row()->decoder()->name());    // Column #2, Decoder
	case 3: return QVariant(ann->row()->description());        // Column #3, Ann Row
	case 4: return QVariant(ann->ann_class_description());     // Column #4, Ann Class
	case 5: return QVariant(ann->longest_annotation());        // Column #5, Value
	case 6: return QVariant((qulonglong)ann->end_sample());    // Column #6, End Sample
	default: return QVariant();
	}
}

QVariant AnnotationCollectionModel::data(const QModelIndex& index, int role) const
{
	if (!signal_ || !index.isValid() || !index.internalPointer())
		return QVariant();

	const Annotation* ann =
		static_cast<const Annotation*>(index.internalPointer());

	if ((role == Qt::DisplayRole) || (role == Qt::ToolTipRole))
		return data_from_ann(ann, index.column());

	if (role == Qt::ForegroundRole) {
		if (index.column() >= get_hierarchy_level(ann)) {
			// Invert the text color if this cell is highlighted
			const bool must_highlight = (highlight_sample_num_ > 0) &&
				((int64_t)ann->start_sample() <= highlight_sample_num_) &&
				((int64_t)ann->end_sample() >= highlight_sample_num_);

			if (must_highlight) {
				if (GlobalSettings::current_theme_is_dark())
					return QApplication::palette().brush(QPalette::Window);
				else
					return QApplication::palette().brush(QPalette::WindowText);
			}
		}

		return QApplication::palette().brush(QPalette::WindowText);
	}

	if (role == Qt::BackgroundRole) {
		// Only use custom cell background color if column index reached the hierarchy level
		if (index.column() >= get_hierarchy_level(ann)) {

			QColor color;
			const bool must_highlight = (highlight_sample_num_ > 0) &&
				((int64_t)ann->start_sample() <= highlight_sample_num_) &&
				((int64_t)ann->end_sample() >= highlight_sample_num_);

			if (must_highlight)
				color = ann->color();
			else
				color = GlobalSettings::current_theme_is_dark() ?
					ann->dark_color() : ann->bright_color();

			return QBrush(color);
		}
	}

	return QVariant();
}

Qt::ItemFlags AnnotationCollectionModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
}

uint8_t AnnotationCollectionModel::first_hidden_column() const
{
	return first_hidden_column_;
}

QVariant AnnotationCollectionModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if ((section < 0) || (section >= (int)header_data_.size()))
		return QVariant();

	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
		return header_data_[section];

	return QVariant();
}

QModelIndex AnnotationCollectionModel::index(int row, int column,
	const QModelIndex& parent_idx) const
{
	(void)parent_idx;
	assert(column >= 0);

	if (row < 0)
		return QModelIndex();

	QModelIndex idx;

	if ((size_t)row < visible_annotations_.size())
		idx = createIndex(row, column, (void*)visible_annotations_.at(row));

	return idx;
}

QModelIndex AnnotationCollectionModel::parent(const QModelIndex& index) const
{
	(void)index;

	return QModelIndex();
}

int AnnotationCollectionModel::rowCount(const QModelIndex& parent_idx) const
{
	(void)parent_idx;

	return visible_annotations_.size();
}

int AnnotationCollectionModel::columnCount(const QModelIndex& parent_idx) const
{
	(void)parent_idx;

	return header_data_.size();
}

void AnnotationCollectionModel::set_signal_and_segment(data::DecodeSignal* signal, uint32_t current_segment)
{
	qDebug("%s %p %u", __func__, signal, current_segment);
	layoutAboutToBeChanged();

	if (!signal) {
		all_annotations_ = nullptr;
		visible_annotations_.clear();
		signal_ = nullptr;

		dataChanged(QModelIndex(), QModelIndex());
		layoutChanged();
		return;
	}

	all_annotations_ = signal->get_all_annotations_by_segment(current_segment);
	output_mutex_ = &signal->get_output_mutex();
	if (all_annotations_)
		qDebug("size %zu", all_annotations_->size());
	signal_ = signal;

	prev_last_row_ = 0;
	prev_last_row_index_ = 0;
	visible_annotations_.clear();
	update_data();

	prev_segment_ = current_segment;
}

void AnnotationCollectionModel::update_data() {
	update_visible_annotations();

	if (visible_annotations_.empty()) {
		return;
	}

	const size_t new_row_count = visible_annotations_.size() - 1;

	// Force the view associated with this model to update when we have more annotations
	dataChanged(index(0, 0), index(new_row_count, 0));
	layoutChanged();

	prev_last_row_ = new_row_count;
}

void AnnotationCollectionModel::set_visible_classes(const std::unordered_set<decltype(AnnotationClassId::id)> &ann_class_ids)
{
	qDebug(__func__);
	if (ann_class_ids == visible_ann_class_ids_) {
		qDebug("No change");
		return;
	}

	layoutAboutToBeChanged();

	visible_ann_class_ids_ = ann_class_ids;

	prev_last_row_ = 0;
	prev_last_row_index_ = 0;
	visible_annotations_.clear();
	update_data();
}

void AnnotationCollectionModel::update_visible_annotations()
{
	uint64_t count = visible_annotations_.size();
	uint64_t processed = 0;

	if (!output_mutex_ || !all_annotations_) {
		visible_annotations_.clear();
		return;
	}

	// Take the lock to avoid all_annotations being mutated (specifically, insertions not at the end) while we're iterating it
	std::lock_guard<mutex> lock(*output_mutex_);

	if (all_annotations_->empty()) {
		visible_annotations_.clear();
		return;
	}

	qDebug("Start annotation update");
	for (uint64_t i = prev_last_row_index_; i < all_annotations_->size(); i++) {
		const auto* ann = (*all_annotations_)[i];
		AnnotationClassId id {{ann->row()->decoder()->get_stack_level(), ann->ann_class_id()}};
		if (!visible_ann_class_ids_.count(id.id))
			continue;

		if (visible_annotations_.size() < (count + 100))
			visible_annotations_.resize(count + 100);

		visible_annotations_[count++] = ann;
		prev_last_row_index_ = i;
		processed++;
	}
	qDebug("Finish annotation update: %lu %lu", processed, count);

	visible_annotations_.resize(count);
}

QModelIndex AnnotationCollectionModel::update_highlighted_rows(QModelIndex first,
	QModelIndex last, int64_t sample_num)
{
	bool has_highlight = false;
	QModelIndex result;

	highlight_sample_num_ = sample_num;

	if (visible_annotations_.empty())
		return result;

	if (sample_num >= 0) {
		last = last.sibling(last.row() + 1, 0);

		// Check if there are any annotations visible in the table view that
		// we would need to highlight - only then do we do so
		QModelIndex index = first;
		do {
			const Annotation* ann =	static_cast<const Annotation*>(index.internalPointer());
			if (!ann)  // Can happen if the table is being modified at this exact time
				return result;

			if (((int64_t)ann->start_sample() <= sample_num) &&
				((int64_t)ann->end_sample() >= sample_num)) {
				result = index;
				has_highlight = true;
				break;
			}

			index = index.sibling(index.row() + 1, 0);
		} while (index != last);
	}

	if (has_highlight || had_highlight_before_)
		dataChanged(first, last);

	had_highlight_before_ = has_highlight;

	return result;
}

} // namespace tabular_decoder
} // namespace views
} // namespace pv
