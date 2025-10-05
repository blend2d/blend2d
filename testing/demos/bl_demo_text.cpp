#include <blend2d.h>
#include <chrono>

#include "bl_qt_headers.h"
#include "bl_qt_canvas.h"

#include <QRegularExpression>

class PerformanceTimer {
public:
  typedef std::chrono::high_resolution_clock::time_point TimePoint;

  TimePoint _start_time {};
  TimePoint _end_time {};

  inline void start() {
    _start_time = std::chrono::high_resolution_clock::now();
  }

  inline void stop() {
    _end_time = std::chrono::high_resolution_clock::now();
  }

  inline double duration() const {
    std::chrono::duration<double> elapsed = _end_time - _start_time;
    return elapsed.count() * 1000;
  }
};

static void debug_glyph_buffer_sink(const char* message, size_t size, void* user_data) noexcept {
  BLString* buffer = static_cast<BLString*>(user_data);
  buffer->append(message, size);
  buffer->append('\n');
}

static bool is_tag_char(char c) noexcept { return uint8_t(c) >= 32u && uint8_t(c) < 128u; }

static BLFontFeatureSettings parse_font_features(const QString& s) {
  BLFontFeatureSettings settings;
  QStringList parts = s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

  for (const QString& part : parts) {
    if (part.length() < 6u)
      continue;

    char tag0 = part[0].toLatin1();
    char tag1 = part[1].toLatin1();
    char tag2 = part[2].toLatin1();
    char tag3 = part[3].toLatin1();
    char eq   = part[4].toLatin1();

    if (is_tag_char(tag0) && is_tag_char(tag1) && is_tag_char(tag2) && is_tag_char(tag3) && eq == '=') {
      BLTag feature_tag = BL_MAKE_TAG(uint8_t(tag0), uint8_t(tag1), uint8_t(tag2), uint8_t(tag3));
      QString feature_value = part.sliced(5);

      bool ok;
      unsigned unsigned_value = feature_value.toUInt(&ok);

      if (ok) {
        settings.set_value(feature_tag, unsigned_value);
      }
    }
  }

  return settings;
}

class MainWindow : public QWidget {
  Q_OBJECT

public:
  // Widgets.
  QComboBox* _renderer_select {};
  QComboBox* _style_select {};
  QLineEdit* _file_selected {};
  QPushButton* _file_selected_button {};
  QSlider* _slider {};
  QLineEdit* _text {};
  QLineEdit* _features_list {};
  QLineEdit* _features_select {};
  QBLCanvas* _canvas {};
  QCheckBox* _ot_debug {};

  int _qt_application_font_id = -1;

  // Loaded font.
  BLFontFace _bl_face;
  QFont _qt_font;
  QRawFont _qt_raw_font;

  MainWindow() {
    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    _renderer_select = new QComboBox();
    QBLCanvas::init_renderer_select_box(_renderer_select);

    _style_select = new QComboBox();
    _style_select->addItem("Solid Color", QVariant(int(0)));
    _style_select->addItem("Linear Gradient", QVariant(int(1)));
    _style_select->addItem("Radial Gradient", QVariant(int(2)));
    _style_select->addItem("Conic Gradient", QVariant(int(3)));

    _file_selected = new QLineEdit("");
    _file_selected_button = new QPushButton("Select...");
    _slider = new QSlider();
    _canvas = new QBLCanvas();

    _slider->setOrientation(Qt::Horizontal);
    _slider->setMinimum(5);
    _slider->setMaximum(400);
    _slider->setSliderPosition(20);

    _text = new QLineEdit();
    _text->setText(QString("Test"));

    _features_list = new QLineEdit();
    _features_list->setReadOnly(true);

    _features_select = new QLineEdit();

    _ot_debug = new QCheckBox();
    _ot_debug->setText(QLatin1String("OpenType Dbg"));

    connect(_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));
    connect(_style_select, SIGNAL(activated(int)), SLOT(onStyleChanged(int)));
    connect(_ot_debug, SIGNAL(stateChanged(int)), SLOT(valueChanged(int)));
    connect(_file_selected_button, SIGNAL(clicked()), SLOT(selectFile()));
    connect(_file_selected, SIGNAL(textChanged(const QString&)), SLOT(fileChanged(const QString&)));
    connect(_slider, SIGNAL(valueChanged(int)), SLOT(valueChanged(int)));
    connect(_text, SIGNAL(textChanged(const QString&)), SLOT(textChanged(const QString&)));
    connect(_features_select, SIGNAL(textChanged(const QString&)), SLOT(textChanged(const QString&)));

    _canvas->on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas->on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(_renderer_select, 0, 1);
    grid->addWidget(_ot_debug, 0, 4);

    grid->addWidget(new QLabel("Style:"), 1, 0);
    grid->addWidget(_style_select, 1, 1);

    grid->addWidget(new QLabel("Font:"), 2, 0);
    grid->addWidget(_file_selected, 2, 1, 1, 3);
    grid->addWidget(_file_selected_button, 2, 4);

    grid->addWidget(new QLabel("Size:"), 3, 0);
    grid->addWidget(_slider, 3, 1, 1, 4);

    grid->addWidget(new QLabel("Font Features:"), 4, 0);
    grid->addWidget(_features_list, 4, 1, 1, 4);

    grid->addWidget(new QLabel("Active FEAT=V "), 5, 0);
    grid->addWidget(_features_select, 5, 1, 1, 4);

    grid->addWidget(new QLabel("Text:"), 6, 0);
    grid->addWidget(_text, 6, 1, 1, 4);

    vBox->addItem(grid);
    vBox->addWidget(_canvas);

    setLayout(vBox);
  }

  void keyPressEvent(QKeyEvent *event) override {}
  void mousePressEvent(QMouseEvent* event) override {}
  void mouseReleaseEvent(QMouseEvent* event) override {}
  void mouseMoveEvent(QMouseEvent* event) override {}

  void reloadFont(const char* file_name) {
    _bl_face.reset();
    if (_qt_application_font_id != -1) {
      QFontDatabase::removeApplicationFont(_qt_application_font_id);
    }

    BLArray<uint8_t> data_buffer;
    if (BLFileSystem::read_file(file_name, data_buffer) == BL_SUCCESS) {
      BLFontData fontData;
      if (fontData.create_from_data(data_buffer) == BL_SUCCESS) {
        _bl_face.create_from_data(fontData, 0);

        BLArray<BLTag> tags;
        _bl_face.get_feature_tags(&tags);

        QString tagsStringified;
        for (BLTag tag : tags) {
          char tag_string[4] = {
            char((tag >> 24) & 0xFF),
            char((tag >> 16) & 0xFF),
            char((tag >>  8) & 0xFF),
            char((tag >>  0) & 0xFF)
          };

          if (!tagsStringified.isEmpty()) {
            tagsStringified.append(QLatin1String(" ", 1));
          }

          tagsStringified.append(QLatin1String(tag_string, 4));
        }

        _features_list->setText(tagsStringified);
      }

      QByteArray qt_buffer(reinterpret_cast<const char*>(data_buffer.data()), data_buffer.size());
      _qt_application_font_id = QFontDatabase::addApplicationFontFromData(qt_buffer);
    }
  }

private Q_SLOTS:
  Q_SLOT void onStyleChanged(int index) { _canvas->update_canvas(); }
  Q_SLOT void onRendererChanged(int index) { _canvas->set_renderer_type(_renderer_select->itemData(index).toInt()); }

  void selectFile() {
    QString file_name = _file_selected->text();
    QFileDialog dialog(this);

    if (!file_name.isEmpty())
      dialog.setDirectory(QFileInfo(file_name).absoluteDir().path());

    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(QString("Font File (*.ttf *.otf)"));
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted) {
      file_name = dialog.selectedFiles()[0];
      _file_selected->setText(file_name);
    }
  }

  void fileChanged(const QString&) {
    QByteArray file_name_utf8 = _file_selected->text().toUtf8();
    file_name_utf8.append('\0');

    reloadFont(file_name_utf8.constData());
    _canvas->update_canvas();
  }

  void valueChanged(int value) {
    _canvas->update_canvas();
  }

  void textChanged(const QString&) {
    _canvas->update_canvas();
  }

public:
  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.fill_all(BLRgba32(0xFF000000));

    int styleId = _style_select->currentIndex();
    BLVar style;

    switch (styleId) {
      default:
      case 0: {
        style = BLRgba32(0xFFFFFFFF);
        break;
      }

      case 1: {
        double w = _canvas->bl_image.width();
        double h = _canvas->bl_image.height();

        BLGradient g(BLLinearGradientValues(0, 0, w, h));
        g.add_stop(0.0, BLRgba32(0xFFFF0000));
        g.add_stop(0.5, BLRgba32(0xFFAF00AF));
        g.add_stop(1.0, BLRgba32(0xFF0000FF));

        style = g;
        break;
      }

      case 2: {
        double w = _canvas->bl_image.width();
        double h = _canvas->bl_image.height();
        double r = bl_min(w, h);

        BLGradient g(BLRadialGradientValues(w * 0.5, h * 0.5, w * 0.5, h * 0.5, r * 0.5));
        g.add_stop(0.0, BLRgba32(0xFFFF0000));
        g.add_stop(0.5, BLRgba32(0xFFAF00AF));
        g.add_stop(1.0, BLRgba32(0xFF0000FF));

        style = g;
        break;
      }

      case 3: {
        double w = _canvas->bl_image.width();
        double h = _canvas->bl_image.height();

        BLGradient g(BLConicGradientValues(w * 0.5, h * 0.5, 0.0));
        g.add_stop(0.00, BLRgba32(0xFFFF0000));
        g.add_stop(0.33, BLRgba32(0xFFAF00AF));
        g.add_stop(0.66, BLRgba32(0xFF0000FF));
        g.add_stop(1.00, BLRgba32(0xFFFF0000));

        style = g;
        break;
      }
    }

    BLFont font;
    BLFontFeatureSettings featureSettings = parse_font_features(_features_select->text());
    font.create_from_face(_bl_face, _slider->value(), featureSettings);

    // Qt uses UTF-16 strings, Blend2D can process them natively.
    QString text = _text->text();
    PerformanceTimer timer;
    timer.start();
    ctx.fill_utf16_text(BLPoint(10, 10 + font.size()), font, reinterpret_cast<const uint16_t*>(text.constData()), text.length(), style);
    timer.stop();

    if (_ot_debug->checkState() == Qt::Checked) {
      BLGlyphBuffer gb;
      BLString output;
      gb.set_debug_sink(debug_glyph_buffer_sink, &output);
      gb.set_utf16_text(reinterpret_cast<const uint16_t*>(text.constData()), text.length());
      font.shape(gb);

      BLFont smallFont;
      smallFont.create_from_face(_bl_face, 22.0f);
      BLFontMetrics metrics = smallFont.metrics();

      size_t i = 0;
      BLPoint pos(10, 10 + font.size() * 1.2 + smallFont.size());
      while (i < output.size()) {
        size_t end = bl_min(output.index_of('\n', i), output.size());

        BLRgba32 color = BLRgba32(0xFFFFFFFF);
        if (end - i > 0 && output.data()[i] == '[')
          color = BLRgba32(0xFFFFFF00);

        ctx.fill_utf8_text(pos, smallFont, output.data() + i, end - i, color);
        pos.y += metrics.ascent + metrics.descent;
        i = end + 1;
      }
    }

    _updateTitle(timer.duration());
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.fillRect(0, 0, _canvas->width(), _canvas->height(), QColor(0, 0, 0));

    if (_qt_application_font_id == -1)
      return;

    int styleId = _style_select->currentIndex();
    QBrush brush;

    switch (styleId) {
      default:
      case 0: {
        brush = QColor(255, 255, 255);
        break;
      }

      case 1: {
        double w = _canvas->bl_image.width();
        double h = _canvas->bl_image.height();

        QLinearGradient g(qreal(0), qreal(0), qreal(w), qreal(h));
        g.setColorAt(0.0f, QColor(0xFF, 0x00, 0x00));
        g.setColorAt(0.5f, QColor(0xAF, 0x00, 0xAF));
        g.setColorAt(1.0f, QColor(0x00, 0x00, 0xFF));

        brush = QBrush(g);
        break;
      }

      case 2: {
        double w = _canvas->bl_image.width();
        double h = _canvas->bl_image.height();
        double r = bl_min(w, h);

        QRadialGradient g(qreal(w * 0.5), qreal(h * 0.5), qreal(r * 0.5), qreal(w * 0.5), qreal(h * 0.5));
        g.setColorAt(0.0f, QColor(0xFF, 0x00, 0x00));
        g.setColorAt(0.5f, QColor(0xAF, 0x00, 0xAF));
        g.setColorAt(1.0f, QColor(0x00, 0x00, 0xFF));

        brush = QBrush(g);
        break;
      }

      case 3: {
        double w = _canvas->bl_image.width();
        double h = _canvas->bl_image.height();

        QConicalGradient g(qreal(w * 0.5), qreal(h * 0.5), 0.0);
        g.setColorAt(0.00f, QColor(0xFF, 0x00, 0x00));
        g.setColorAt(0.66f, QColor(0xAF, 0x00, 0xAF));
        g.setColorAt(0.33f, QColor(0x00, 0x00, 0xFF));
        g.setColorAt(1.00f, QColor(0xFF, 0x00, 0x00));

        brush = QBrush(g);
        break;
      }
    }

    QStringList families = QFontDatabase::applicationFontFamilies(_qt_application_font_id);
    QFont font = QFont(families[0]);
    font.setPixelSize(_slider->value());
    font.setHintingPreference(QFont::PreferNoHinting);
    ctx.setFont(font);

    QPen pen(brush, 1.0f);
    ctx.setPen(pen);

    PerformanceTimer timer;
    timer.start();
    ctx.drawText(QPointF(10, 10 + font.pixelSize()), _text->text());
    timer.stop();
    _updateTitle(timer.duration());
  }

  void _updateTitle(double duration) {
    char buf[256];
    snprintf(buf, 256, "Text Sample [Size %dpx TextRenderTime %0.3fms]", int(_slider->value()), duration);

    QString title = QString::fromUtf8(buf);
    if (title != windowTitle())
      setWindowTitle(title);
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow win;

  win.resize(QSize(580, 520));
  win.show();

  return app.exec();
}

#include "bl_demo_text.moc"
