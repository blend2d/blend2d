#include <blend2d.h>

#include "bl_qt_headers.h"
#include "bl_qt_canvas.h"

class MainWindow : public QWidget {
  Q_OBJECT

public:
  // Widgets.
  QLineEdit* _file_selected {};
  QPushButton* _file_selected_button {};
  QCheckBox* _animate_check_box {};
  QCheckBox* _background_check_box {};
  QPushButton* _next_frame_button {};
  QBLCanvas* _canvas {};

  BLArray<uint8_t> _image_file_data;
  BLImageDecoder _image_decoder;
  BLImageInfo _loaded_image_info {};
  BLImage _loaded_image;
  BLString _error_message;

  QTimer _timer;

  MainWindow() {
    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    _file_selected = new QLineEdit("");
    _file_selected_button = new QPushButton("Select...");
    _animate_check_box = new QCheckBox("Animate");
    _animate_check_box->setChecked(true);
    _background_check_box = new QCheckBox("White");
    _next_frame_button = new QPushButton("Next");
    _canvas = new QBLCanvas();

    connect(_file_selected_button, SIGNAL(clicked()), SLOT(selectFile()));
    connect(_file_selected, SIGNAL(textChanged(const QString&)), SLOT(fileChanged(const QString&)));
    connect(&_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    connect(_next_frame_button, SIGNAL(clicked()), SLOT(onNextFrame()));
    connect(_background_check_box, SIGNAL(clicked()), SLOT(onRedraw()));

    _canvas->on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);

    grid->addWidget(new QLabel("Image:"), 0, 0);
    grid->addWidget(_file_selected, 0, 1, 1, 3);
    grid->addWidget(_file_selected_button, 0, 4);
    grid->addWidget(_animate_check_box, 0, 5);
    grid->addWidget(_background_check_box, 0, 6);
    grid->addWidget(_next_frame_button, 0, 7);

    vBox->addItem(grid);
    vBox->addWidget(_canvas);

    setLayout(vBox);
    _timer.setInterval(50);
  }

  void keyPressEvent(QKeyEvent *event) override {}
  void mousePressEvent(QMouseEvent* event) override {}
  void mouseReleaseEvent(QMouseEvent* event) override {}
  void mouseMoveEvent(QMouseEvent* event) override {}

  bool create_decoder(const char* file_name) {
    _image_file_data.reset();
    _image_decoder.reset();
    _loaded_image_info.reset();
    _loaded_image.reset();
    _error_message.clear();

    if (BLFileSystem::read_file(file_name, _image_file_data) != BL_SUCCESS) {
      _error_message.assign("Failed to read the file specified");
      return false;
    }

    BLImageCodec codec;
    if (codec.find_by_data(_image_file_data) != BL_SUCCESS) {
      _error_message.assign("Failed to find a codec for the given file");
      return false;
    }

    if (codec.create_decoder(&_image_decoder) != BL_SUCCESS) {
      _error_message.assign("Failed to create a decoder for the given file");
      return false;
    }

    if (_image_decoder.read_info(_loaded_image_info, _image_file_data) != BL_SUCCESS) {
      _error_message.assign("Failed to read image information");
      return false;
    }

    return true;
  }

  bool read_frame() {
    if (_image_decoder.read_frame(_loaded_image, _image_file_data) != BL_SUCCESS) {
      _error_message.assign("Failed to decode the image");
      return false;
    }

    return true;
  }

  void load_image(const char* file_name) {
    if (create_decoder(file_name)) {
      if (read_frame()) {
        if (_loaded_image_info.frame_count > 1u) {
          _timer.start();
        }
      }
    }

    _updateTitle();
  }

  Q_SLOT void selectFile() {
    QString file_name = _file_selected->text();
    QFileDialog dialog(this);

    if (!file_name.isEmpty())
      dialog.setDirectory(QFileInfo(file_name).absoluteDir().path());

    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(QString("Image File (*.apng *.bmp *.jpeg *.jpg *.png *.qoi)"));
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted) {
      file_name = dialog.selectedFiles()[0];
      _file_selected->setText(file_name);
    }
  }

  Q_SLOT void fileChanged(const QString&) {
    QByteArray file_name_utf8 = _file_selected->text().toUtf8();
    file_name_utf8.append('\0');

    load_image(file_name_utf8.constData());
    _canvas->update_canvas();
  }

  Q_SLOT void onTimer() {
    if (!_animate_check_box->isChecked()) {
      return;
    }

    if (read_frame()) {
      _canvas->update_canvas();
    }
    else {
      _timer.stop();
    }
  }

  Q_SLOT void onNextFrame() {
    if (_loaded_image_info.frame_count > 1u) {
      if (read_frame()) {
        _canvas->update_canvas();
      }
    }
  }

  Q_SLOT void onRedraw() {
    _canvas->update_canvas();
  }

  void on_render_blend2d(BLContext& ctx) noexcept {
    BLRgba32 background = _background_check_box->isChecked() ? BLRgba32(0xFFFFFFFFu) : BLRgba32(0xFF000000u);

    ctx.fill_all(background);
    ctx.blit_image(BLPointI(0, 0), _loaded_image);
  }

  void _updateTitle() {
    BLString s;

    if (!_error_message.is_empty()) {
      s.assign_format("Load ERROR=%s", _error_message.data());
    }
    else {
      s.assign_format("Image Size=[%dx%d] Format=%s Depth=%u Compression=%s Frames=%llu",
        _loaded_image.width(),
        _loaded_image.height(),
        _loaded_image_info.format,
        _loaded_image_info.depth,
        _loaded_image_info.compression,
        (unsigned long long)_loaded_image_info.frame_count);
    }

    setWindowTitle(QString::fromUtf8(s.data()));
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow win;

  win.resize(QSize(580, 520));
  win.show();

  return app.exec();
}

#include "bl_demo_image_view.moc"
