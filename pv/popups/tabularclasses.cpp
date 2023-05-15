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

#include <QApplication>
//#include <QFormLayout>
#include <QGridLayout>
//#include <QHBoxLayout>
#include <QLabel>

#include "tabularclasses.hpp"

#include <pv/session.hpp>

namespace pv::popups {

TabularClasses::TabularClasses(Session &session, QWidget *parent, QListWidget* list) :
	Popup(parent),
	session_(session),
	list_(list)
{
	// Create the layout
	setLayout(&layout_);

	layout_.addItem(new QSpacerItem(0, 15, QSizePolicy::Expanding, QSizePolicy::Expanding));
	layout_.addRow(list_);
}

}  // namespace pv
