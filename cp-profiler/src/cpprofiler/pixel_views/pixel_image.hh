#pragma once

#include <QObject>

#include <cstdint>
#include <vector>
#include <memory>
#include <QRgb>

class QImage;

namespace cpprofiler
{

namespace pixel_view
{

constexpr static int DEFAULT_PIXEL_SIZE = 10;

class PixelImage : public QObject
{
  Q_OBJECT
private:
  /// Buffer used to initialize QImage
  std::vector<uint32_t> buffer_;

  /// The image used for diplaying the tree
  std::unique_ptr<QImage> image_;

  int width_ = 40;
  int height_ = 20;

  int pixel_size_ = DEFAULT_PIXEL_SIZE;

  void setPixel(std::vector<uint32_t> &buffer, int x, int y, QRgb color);

public:
  PixelImage();

  ~PixelImage();

  /// Set all pixels to a default color
  void clear();
  /// change QImage to match the buffer
  void update();

  void resize(const QSize &size);

  void drawPixel(int x, int y, QRgb color);

  void drawRect(int x, int y, int width, QRgb color);

  const QImage &raw_image() const
  {
    return *image_;
  }

  void setPixelSize(int size) { pixel_size_ = size; }

  int pixel_size() const { return pixel_size_; }

public slots:

  /// Decrease pixel size
  void zoomOut();

  /// Increase pixel size
  void zoomIn();
};
} // namespace pixel_view
} // namespace cpprofiler