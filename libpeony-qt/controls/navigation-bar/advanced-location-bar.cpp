/*
 * Peony-Qt's Library
 *
 * Copyright (C) 2019, Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: Yue Lan <lanyue@kylinos.cn>
 *
 */

#include "advanced-location-bar.h"
#include "path-edit.h"
#include "location-bar.h"
#include "search-vfs-uri-parser.h"
#include "search-bar-container.h"

#include <QStackedLayout>
#include <QDebug>

using namespace Peony;

AdvancedLocationBar::AdvancedLocationBar(QWidget *parent) : QWidget(parent)
{
    QStackedLayout *layout = new QStackedLayout(this);
    m_layout = layout;

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->setSizeConstraint(QLayout::SetDefaultConstraint);
    m_bar = new Peony::LocationBar(this);
    m_edit = new Peony::PathEdit(this);
    m_search_bar = new Peony::SearchBarContainer(this);
    //bar->connect(bar, &Peony::LocationBar::groupChangedRequest, bar, &Peony::LocationBar::setRootUri);
    m_bar->connect(m_bar, &Peony::LocationBar::blankClicked, [=](){
        layout->setCurrentWidget(m_edit);
        m_edit->setFocus();
        m_edit->setUri(m_bar->getCurentUri());
    });

    m_edit->connect(m_edit, &Peony::PathEdit::uriChangeRequest, [=](const QString uri){
        if (m_text == uri) {
            Q_EMIT this->refreshRequest();
            return;
        }
        m_bar->setRootUri(uri);
        layout->setCurrentWidget(m_bar);
        Q_EMIT this->updateWindowLocationRequest(uri);
        m_text = m_edit->text();
    });

    m_bar->connect(m_bar, &LocationBar::groupChangedRequest, [=](const QString &uri){
        if (m_text == uri) {
            Q_EMIT this->refreshRequest();
            return;
        }
        Q_EMIT this->updateWindowLocationRequest(uri);
        m_text = uri;
    });

    m_edit->connect(m_edit, &Peony::PathEdit::editCancelled, [=](){
        layout->setCurrentWidget(m_bar);
    });

    m_search_bar->connect(m_search_bar, &Peony::SearchBarContainer::returnPressed, [=](){
         //qDebug() << "start search" << m_search_bar->text();
         auto key = m_search_bar->text();
         auto targetUri = Peony::SearchVFSUriParser::parseSearchKey(m_text, key);
         Q_EMIT this->updateWindowLocationRequest(targetUri);

    });

    m_search_bar->connect(m_search_bar, &Peony::SearchBarContainer::filterUpdate, [=](const int &index)
    {
        Q_EMIT this->updateFileTypeFilter(index);
    });


    layout->addWidget(m_bar);
    layout->addWidget(m_edit);
    layout->addWidget(m_search_bar);

    setLayout(layout);
    setFixedHeight(m_edit->height());
}

void AdvancedLocationBar::updateLocation(const QString &uri)
{
    //qDebug() << "AdvancedLocationBar updateLocation:"<<uri;
    m_bar->setRootUri(uri);
    m_edit->setUri(uri);
    m_text = uri;
    Q_EMIT this->refreshRequest();
}

bool AdvancedLocationBar::isEditing()
{
    return m_edit->isVisible();
}

void AdvancedLocationBar::startEdit()
{
    m_layout->setCurrentWidget(m_edit);
    m_edit->setFocus();
    m_edit->setUri(m_bar->getCurentUri());
}

void AdvancedLocationBar::finishEdit()
{
    Q_EMIT m_edit->returnPressed();
}

void AdvancedLocationBar::switchEditMode(bool bSearchMode)
{
    if (bSearchMode)
    {
        m_layout->setCurrentWidget(m_search_bar);
        m_search_bar->setPlaceholderText(tr("Search Content..."));
        m_search_bar->setFocus();
    }
    else
    {
        m_layout->setCurrentWidget(m_bar);
    }
}
