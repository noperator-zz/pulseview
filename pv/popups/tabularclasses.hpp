/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2023 Ivan Chichkine <chichkine@yahoo.com>
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

#ifndef PULSEVIEW_PV_POPUPS_TABULAR_CLASSES_HPP
#define PULSEVIEW_PV_POPUPS_TABULAR_CLASSES_HPP

#include <QListWidget>
#include <QFormLayout>
#include <QGridLayout>

#include <pv/widgets/popup.hpp>

namespace pv {

class Session;

namespace popups {

class TabularClasses : public pv::widgets::Popup
{
	Q_OBJECT

public:
	TabularClasses(Session &session, QWidget *parent, QListWidget* list);

private:
	pv::Session &session_;

	QFormLayout layout_;

	QListWidget* list_;
};

}  // namespace popups
}  // namespace pv

#endif // PULSEVIEW_PV_POPUPS_TABULAR_CLASSES_HPP
