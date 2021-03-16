#include "widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    band = new QRubberBand(QRubberBand::Rectangle, this);
    _shouldRelease = true;

    _values_freq_map.clear();

    resize2DVector(_values, this->size());
    resize2DVector(_already_done, this->size());

    QThreadPool::globalInstance()->setExpiryTimeout(-1);

    this->_callNumber = 0;
    _values_lock = new QRecursiveMutex();

    _area = QRectF(-2.5, -1.125, 4.0, 2.5);

    this->resize(1600, 900);
    this->show();
}

int Widget::calculatePoint(QPoint point) {
    long double z = 0, zi = 0;
    long double tmp;

    long double x = _area.x() + _area.width() * point.x() / this->width();
    long double y = _area.y() + _area.height() * point.y() / this->height();

    int cnt;
    for (cnt = 0; cnt < Widget::ITER_CNT; ++cnt) {
        tmp = z * z - zi * zi + x;
        zi = 2 * z * zi + y;
        z = tmp;

        if (z * z + zi * zi > 4)
            break;
    }

    return cnt;
}

void Widget::calculateALittleBit() {
    int w = this->width(), h = this->height();
    int hZonesCnt = w / Widget::HSCALE;
    int vZonesCnt = h / Widget::VSCALE;

    for (int i = 0; i < hZonesCnt; ++i) {
        for (int j = 0; j < vZonesCnt; ++j) {
            int left = w * i / hZonesCnt, right = w * (i + 1) / hZonesCnt;
            int bottom = h * j / vZonesCnt, top = h * (j + 1) / vZonesCnt;

            int cnt = calculatePoint(QPoint(left, bottom));
            if (_values_freq_map.find(cnt) != _values_freq_map.end())
                _values_freq_map[cnt]++;
            else
                _values_freq_map[cnt] = 1;

            _already_done[left][bottom] = 1;

            for (int p = left; p < right; ++p)
                for (int q = bottom; q < top; ++q)
                    _values[p][q] = cnt;
        }
    }
}

void Widget::calculateRect(QRect rect, unsigned callNumber) {
    for (int i = rect.x(); i < rect.x() + rect.width(); ++i) {
        for (int j = rect.y(); j < rect.y() + rect.height(); ++j) {

            bool done = false;
            {
                QMutexLocker lock(_values_lock);
                if (this->_callNumber == callNumber)
                    done = _already_done[i][j];
                else
                    return;
            }

            if (done)
                continue;

            int cnt = calculatePoint(QPoint(i, j));
            {
                QMutexLocker lock(_values_lock);
                if (this->_callNumber == callNumber) {
                    _values[i][j] = cnt;

                    if (_values_freq_map.find(cnt) != _values_freq_map.end())
                        _values_freq_map[cnt]++;
                    else
                        _values_freq_map[cnt] = 1;
                }
                else
                    return;
            }
       }
    }

    QMutexLocker lock(_values_lock);
    if (this->_callNumber == callNumber) {
        _activeTasks--;
        this->update();
    }
}

void Widget::startTasks() {
    QVector <std::function <void()>> tasks;

    int w = this->width();
    int h = this->height();

    int N = Widget::TASKS_SCALE;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {

            int left = w * i / N, right = w * (i + 1) / N;
            int bottom = h * j / N, top = h * (j + 1) / N;

            tasks.append(std::function<void()>([=]() {
                this->calculateRect(QRect(left, bottom, right - left, top - bottom), _callNumber);
            }));
        }
    }

    this->_ready = false;
    this->_activeTasks = N * N;

    for (auto task : tasks)
        QThreadPool::globalInstance()->start(task);
}

void Widget::normalizeValues() {
    if (_values_freq_map.size() < Widget::COLORS_CNT) {
        int cnt = 0;

        for (auto v : _values_freq_map) {
            _values_normalize_map[v.first] = cnt++ * Widget::COLORS_CNT / _values_freq_map.size();
        }

        return;
    }

    QVector<int> freq;
    for (auto v : _values_freq_map)
        freq.append(v.second);

    std::sort(freq.begin(), freq.end());

    int minInd = freq.size() - Widget::COLORS_CNT, minCnt = 1;
    while (minInd + minCnt < freq.size() &&
           freq[minInd + minCnt] == freq[minInd + minCnt - 1]) {
        ++minCnt;
    }

    int cnt = 0;
    for (auto v : _values_freq_map) {
        _values_normalize_map[v.first] = cnt;

        if (v.second > freq[minInd] || (v.second == freq[minInd] && minCnt > 1)) {
            if (v.second == freq[minInd] && minCnt > 1)
                --minCnt;
            cnt++;
        }
    }
}

void Widget::calculateEverything() {
    _values_freq_map.clear();
    for (auto &i : _already_done)
        i.fill(0);

    calculateALittleBit();

    startTasks();

    this->update();
}

void Widget::resizeEvent(QResizeEvent *event) {
    {
        QMutexLocker lock(_values_lock);

        _callNumber += 1;
        QThreadPool::globalInstance()->clear();

        resize2DVector(_values, this->size());
        resize2DVector(_already_done, this->size());

        image = QImage(this->size(), QImage::Format_RGB32);
    }

    calculateEverything();
}

void Widget::paintEvent(QPaintEvent *event) {
    normalizeValues();

    auto color = [=] (int value) {
        return QColor(value * 255 / Widget::COLORS_CNT, value * 255 / Widget::COLORS_CNT, value * 255 / Widget::COLORS_CNT);
    };

    QPainter painter(this);
    if (!_ready) {
        for (int i = 0; i < this->width(); ++i)
            for (int j = 0; j < this->height(); ++j)
                image.setPixelColor(i, j,  color(_values_normalize_map[_values[i][j]]));
    }

    if (_activeTasks == 0)
        _ready = true;

    painter.drawImage(0, 0, image);
}

void Widget::mousePressEvent(QMouseEvent *event) {
    if (!_ready) {
        _shouldRelease = false;
        return;
    }

    _shouldRelease = true;

    origin = event->pos();
    if (!band)
        band = new QRubberBand(QRubberBand::Rectangle, this);

    band->setGeometry(QRect(origin, QSize()));
    band->show();
}

void Widget::mouseMoveEvent(QMouseEvent *event) {
    QPoint pos = event->pos();

    int ws = 1, hs = 1;
    if (pos.x() < origin.x()) {
        ws = -1;
        pos.setX(pos.x() + 2 * (origin.x() - pos.x()));
    }
    if (pos.y() < origin.y()) {
        hs = -1;
        pos.setY(pos.y() + 2 * (origin.y() - pos.y()));
    }

    QRect rect(origin, pos);

    int rw = rect.width(), rh = rect.height();
    int w = this->width(), h = this->height();

    if (rw * h < w * rh)
        rect.setWidth(rh * w / h);
    else
        rect.setHeight(rw * h / w);

    rect.setHeight(rect.height() * hs);
    rect.setWidth(rect.width() * ws);

    band->setGeometry(rect.normalized());
}

void Widget::mouseReleaseEvent(QMouseEvent *event) {
    if (!_shouldRelease)
        return;

    band->hide();

    if (event->pos() == origin)
        return;

    double newWidth = _area.width() * band->width() / this->width();
    double newHeight = _area.height() * band->height() / this->height();
    double newX = _area.x() + _area.width() * band->x() / this->width();
    double newY = _area.y() + _area.height() * band->y() / this->height();

    _area = QRectF(newX, newY, newWidth, newHeight);

    calculateEverything();
}

Widget::~Widget() {
    QThreadPool::globalInstance()->waitForDone();
    delete band;
    delete _values_lock;
}

