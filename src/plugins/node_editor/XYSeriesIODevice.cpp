/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "XYSeriesIODevice.h"
#include<QTimer>
#include<QDebug>
#include<QMutexLocker>

static inline unsigned long near_power_of_two(unsigned long x) {
    unsigned long cnt = 0;
    while( x > 0 ) {
        x = x >> 1;
        cnt++;
    }
    return 1 << cnt;
}

XYSeriesIODevice::XYSeriesIODevice(QObject *parent) :
    QIODevice(parent)
{
    initParams(2, 2, 8000);
    qRegisterMetaType<QVector<QPointF>>("QVector<QPointF>");
    QTimer::singleShot(100, this, SLOT(pollData()));
}

XYSeriesIODevice::~XYSeriesIODevice()
{
    qDebug() << "Destroy XYSeriesIODevice" << this;
}

/**
 * @brief XYSeriesIODevice::pollData
 * Read data from ring buffer and send data to screen on regular time (25 ms).
 * This is to have a evenly refresh of the screen
 */
void XYSeriesIODevice::pollData(){

    QMutexLocker locker(&m_lock);
    qint64 len = (m_write_idx - m_read_idx) & m_mask;

    if(len > 0) {

        if (m_buffer.isEmpty()) {
            m_buffer.reserve(m_sampleCount);
            for (int i = 0; i < m_sampleCount; ++i)
                m_buffer.append(QPointF(i, 0));
        }

        int start = 0;
        int s;
        int offset = m_channels*m_resolution;
        int availableSamples = len/(offset);
        if (availableSamples < m_sampleCount) {
            start = m_sampleCount - availableSamples;
            for (  s = 0; s < start; ++s)
                m_buffer[s].setY(m_buffer.at(s + availableSamples).y());
        }

        for (  s = start; s < m_sampleCount && m_read_idx!=m_write_idx; ++s){
            int a = (*reinterpret_cast<const short int*>(&(m_data[m_read_idx])));
            m_buffer[s].setY((qreal(a)/200));
            m_read_idx = (m_read_idx + offset) & m_mask;
        }

        if(m_read_idx!=m_write_idx){
            qDebug() << "Possible Issue R " << m_read_idx <<  "W " << m_write_idx;
            m_read_idx=m_write_idx;
        }

        //  m_series->replace(m_buffer);
        emit bufferReady(m_buffer, 0 );
    }
    else{
        // qDebug() << "DN";
    }

    QTimer::singleShot(25, this, SLOT(pollData()));
}

qint64 XYSeriesIODevice::readData(char *data, qint64 maxSize)
{
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    return -1;
}

qint64 XYSeriesIODevice::writeData(const char *data, qint64 maxSize)
{
    QMutexLocker locker(&m_lock);

    bool overrun = ((m_read_idx -1 - m_write_idx) & m_mask) < maxSize;
    for(int i=0 ; i< maxSize; i++ ) {
        m_data[m_write_idx] = data[i];
        m_write_idx = (m_write_idx + 1) & m_mask;
    }

    if(overrun) {
        qDebug() << "Ring buffer overrun";
        m_read_idx = (m_write_idx-maxSize)&m_mask;
    }
    return maxSize;
}

void XYSeriesIODevice::initParams(int resolution_bytes, int channels, int sampleCount)
{
    QMutexLocker locker(&m_lock);
    m_read_idx = 0;
    m_write_idx = 0;
    m_resolution = resolution_bytes;
    m_channels   = channels;
    m_sampleCount = sampleCount;
    m_mask = near_power_of_two(static_cast<unsigned long>(m_sampleCount*m_resolution*m_channels)) - 1;
    m_data = new char [m_mask + 1];
}

