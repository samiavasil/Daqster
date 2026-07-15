#pragma once

#include <QtGlobal>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace QtChartsCompat {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using Chart = ::QChart;
using ChartView = ::QChartView;
using Legend = ::QLegend;
using LineSeries = ::QLineSeries;
using ValueAxis = ::QValueAxis;
#else
using Chart = QtCharts::QChart;
using ChartView = QtCharts::QChartView;
using Legend = QtCharts::QLegend;
using LineSeries = QtCharts::QLineSeries;
using ValueAxis = QtCharts::QValueAxis;
#endif

} // namespace QtChartsCompat