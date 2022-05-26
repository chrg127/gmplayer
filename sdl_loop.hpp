#include <QObject>

class SDLEventLoop : public QObject {
    Q_OBJECT
public:
    SDLEventLoop(QWidget *parent = nullptr) : QObject(parent) { }
signals:
    void ended(int r);
};
