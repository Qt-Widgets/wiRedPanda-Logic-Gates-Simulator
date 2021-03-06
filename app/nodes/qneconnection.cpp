/* Copyright (c) 2012, STANISLAW ADASZEWSKI
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * Neither the name of STANISLAW ADASZEWSKI nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL STANISLAW ADASZEWSKI BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "common.h"
#include "qneconnection.h"
#include "qneport.h"
#include "thememanager.h"

#include <QBrush>
#include <QDebug>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>

QNEConnection::QNEConnection(QGraphicsItem *parent)
    : QGraphicsPathItem(parent)
    , m_start(nullptr)
    , m_end(nullptr)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setBrush(Qt::NoBrush);
    setStatus(Status::Inactive);
    setZValue(-1);
    updateTheme();
}

QNEConnection::~QNEConnection()
{
    if (m_start) {
        m_start->disconnect(this);
    }
    if (m_end) {
        m_end->disconnect(this);
    }
}

void QNEConnection::setStartPos(const QPointF &p)
{
    m_startPos = p;
}

void QNEConnection::setEndPos(const QPointF &p)
{
    m_endPos = p;
}

void QNEConnection::setStart(QNEOutputPort *p)
{
    QNEOutputPort *old = m_start;
    m_start = p;
    if (old && (old != p)) {
        old->disconnect(this);
    }
    if (p) {
        setStartPos(p->scenePos());
        p->connect(this);
    }
}

void QNEConnection::setEnd(QNEInputPort *p)
{
    QNEInputPort *old = m_end;
    m_end = p;
    if (old && (old != p)) {
        old->disconnect(this);
    }
    if (p) {
        p->connect(this);
        setEndPos(p->scenePos());
    }
}

void QNEConnection::updatePosFromPorts()
{
    if (m_start) {
        m_startPos = m_start->scenePos();
    }
    if (m_end) {
        m_endPos = m_end->scenePos();
    }
    updatePath();
}

void QNEConnection::updatePath()
{
    QPainterPath p;

    p.moveTo(m_startPos);

    qreal dx = m_endPos.x() - m_startPos.x();
    qreal dy = m_endPos.y() - m_startPos.y();

    QPointF ctr1(m_startPos.x() + dx * 0.25, m_startPos.y() + dy * 0.1);
    QPointF ctr2(m_startPos.x() + dx * 0.75, m_startPos.y() + dy * 0.9);

    p.cubicTo(ctr1, ctr2, m_endPos);

    /*  p.lineTo(pos2); */
    setPath(p);
}

QNEOutputPort *QNEConnection::start() const
{
    return m_start;
}

QNEInputPort *QNEConnection::end() const
{
    return m_end;
}

double QNEConnection::angle()
{
    QNEPort *p1 = m_start;
    QNEPort *p2 = m_end;
    if (p1 && p2) {
        if (p2->isOutput()) {
            std::swap(p1, p2);
        }
        QLineF line(p1->scenePos(), p2->scenePos());
        return line.angle();
    }
    return 0.0;
}

void QNEConnection::save(QDataStream &ds) const
{
    ds << reinterpret_cast<quint64>(m_start);
    ds << reinterpret_cast<quint64>(m_end);
}

bool QNEConnection::load(QDataStream &ds, const QMap<quint64, QNEPort *> &portMap)
{
    quint64 ptr1;
    quint64 ptr2;
    ds >> ptr1;
    ds >> ptr2;
    if (portMap.isEmpty()) {
        COMMENT("Empty port map.", 0);
        auto *port1 = reinterpret_cast<QNEPort *>(ptr1);
        auto *port2 = reinterpret_cast<QNEPort *>(ptr2);
        if (port2 && port1) {
            if (!port1->isOutput() && port2->isOutput()) {
                setStart(dynamic_cast<QNEOutputPort *>(port2));
                setEnd(dynamic_cast<QNEInputPort *>(port1));
            } else if (port1->isOutput() && !port2->isOutput()) {
                setStart(dynamic_cast<QNEOutputPort *>(port1));
                setEnd(dynamic_cast<QNEInputPort *>(port2));
            }
        }
    } else if (portMap.contains(ptr1) && portMap.contains(ptr2)) {
        COMMENT("Port map with elements.", 0);
        QNEPort *port1 = portMap[ptr1];
        QNEPort *port2 = portMap[ptr2];
        if (port1 && port2) {
            if (!port1->isOutput() && port2->isOutput()) {
                setStart(dynamic_cast<QNEOutputPort *>(port2));
                setEnd(dynamic_cast<QNEInputPort *>(port1));
            } else if (port1->isOutput() && !port2->isOutput()) {
                setStart(dynamic_cast<QNEOutputPort *>(port1));
                setEnd(dynamic_cast<QNEInputPort *>(port2));
            }
        }
    } else {
        return false;
    }
    updatePosFromPorts();
    updatePath();

    return (m_start != nullptr && m_end != nullptr);
}

QNEPort *QNEConnection::otherPort(const QNEPort *port) const
{
    if (port == dynamic_cast<QNEPort *>(m_start)) {
        return dynamic_cast<QNEPort *>(m_end);
    }
    return dynamic_cast<QNEPort *>(m_start);
}

QNEOutputPort *QNEConnection::otherPort(const QNEInputPort *) const
{
    return m_start;
}

QNEInputPort *QNEConnection::otherPort(const QNEOutputPort *) const
{
    return m_end;
}

QNEConnection::Status QNEConnection::status() const
{
    return m_status;
}

void QNEConnection::setStatus(const Status &status)
{
    m_status = status;
    switch (status) {
    case Status::Inactive: {
        setPen(QPen(m_inactiveClr, 3));
        break;
    }
    case Status::Active: {
        setPen(QPen(m_activeClr, 3));
        break;
    }
    case Status::Invalid: {
        setPen(QPen(m_invalidClr, 5));
        break;
    }
    }
}

void QNEConnection::updateTheme()
{
    if (ThemeManager::globalMngr) {
        const ThemeAttrs attrs = ThemeManager::globalMngr->getAttrs();
        m_inactiveClr = attrs.qneConnection_false;
        m_activeClr = attrs.qneConnection_true;
        m_invalidClr = attrs.qneConnection_invalid;
        m_selectedClr = attrs.qneConnection_selected;
    }
}

void QNEConnection::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setClipRect(option->exposedRect);
    if (isSelected()) {
        painter->setPen(QPen(m_selectedClr, 5));
    } else {
        painter->setPen(pen());
    }
    painter->drawPath(path());
}
