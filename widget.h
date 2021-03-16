#ifndef WIDGET_H
#define WIDGET_H

#include <QtWidgets>

#include <iostream>
#include <map>

class Widget : public QWidget
{
    Q_OBJECT

private:
    enum {
        HSCALE = 16,
        VSCALE = 9,
        TASKS_SCALE = 16,
        COLORS_CNT = 150,
        ITER_CNT = 5000
    };

    QRubberBand *band;
    bool _shouldRelease;

    QPoint origin;
    QRectF _area;

    std::map<int, int> _values_freq_map;
    std::map<int, int> _values_normalize_map;

    QVector<QVector<int>> _values;
    QVector<QVector<bool>> _already_done;
    QImage image;

    bool _ready = 0;
    volatile unsigned _callNumber, _activeTasks;
    QRecursiveMutex *_values_lock;

    int calculatePoint(QPoint point);
    void calculateALittleBit();

    void startTasks();
    void calculateRect(QRect rect, unsigned callNumber);

    template <class T>
    void resize2DVector(QVector<QVector<T>>& vector, QSize newSize) {
        vector.resize(newSize.width());
        for (auto &i : vector)
            i.resize(newSize.height());
    }

    void normalizeValues();
    void calculateEverything();

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

    void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event);

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
};

#endif // WIDGET_H
