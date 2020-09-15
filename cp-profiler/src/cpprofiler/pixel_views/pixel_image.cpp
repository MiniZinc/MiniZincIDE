#include "pixel_image.hh"

#include "../utils/debug.hh"

#include <QImage>
#include <cassert>

namespace cpprofiler
{

namespace pixel_view
{

PixelImage::PixelImage()
{
    image_.reset(new QImage());

    resize({width_, height_});
}

PixelImage::~PixelImage() = default;

void PixelImage::setPixel(std::vector<uint32_t> &buffer, int x, int y,
                          QRgb color)
{
    assert(x >= 0 && y >= 0);

    if (x >= width_ || x < 0 || y >= height_ || y < 0)
    {
        return;
    }

    uint32_t r = qRed(color);
    uint32_t g = qGreen(color);
    uint32_t b = qBlue(color);

    uint32_t pixel_color = (0xFF << 24) + (r << 16) + (g << 8) + (b);

    buffer[y * width_ + x] = pixel_color;
}

void PixelImage::resize(const QSize &size)
{

    width_ = size.width() - 10;
    height_ = size.height() - 10;

    buffer_.clear();
    buffer_.resize(width_ * height_);

    clear();
    update();
}

void PixelImage::clear()
{
    /// set all pixels to white
    std::fill(buffer_.begin(), buffer_.end(), 0xFFFFFF);
}

void PixelImage::update()
{
    auto buf = reinterpret_cast<uint8_t *>(buffer_.data());
    image_.reset(new QImage(buf, width_, height_, QImage::Format_RGB32));
}

void PixelImage::zoomIn()
{
    pixel_size_ += 1;
}

void PixelImage::zoomOut()
{
    if (pixel_size_ > 1)
    {
        --pixel_size_;
    }
}

void PixelImage::drawRect(int x, int y, int width, QRgb color)
{

    const auto BLACK = qRgb(0, 0, 0);
    assert(y >= 0 && width > 0);

    /// the rectange

    if (x < 0)
    {
        /// the rectangle is cut off
        width += x;
        x = 0;
    }

    const int x_begin = x * pixel_size_;
    const int y_begin = y * pixel_size_;
    const int x_end = (x + width) * pixel_size_;
    const int y_end = (y + 1) * pixel_size_;

    /// Horizontal lines
    for (auto col = x_begin; col < x_end; ++col)
    {
        setPixel(buffer_, col, y_begin, BLACK);
    }

    for (auto col = x_begin; col < x_end; ++col)
    {
        setPixel(buffer_, col, y_end - 1, BLACK);
    }

    /// Vertical lines
    for (auto row = y_begin; row < y_end; ++row)
    {
        setPixel(buffer_, x_begin, row, BLACK);
        setPixel(buffer_, x_end - 1, row, BLACK);
    }

    /// Fill the rect
    for (auto col = x_begin + 1; col < x_end - 1; ++col)
    {
        for (auto row = y_begin + 1; row < y_end - 1; ++row)
        {
            setPixel(buffer_, col, row, color);
        }
    }
}

void PixelImage::drawPixel(int x, int y, QRgb color)
{
    assert(x >= 0 && y >= 0);

    int x0 = x * pixel_size_;
    int y0 = y * pixel_size_;

    /// TODO(maxim): experiment with using std::fill to draw rows
    /// TODO(maxim): experiment with drawing row by row
    for (int column = 0; column < pixel_size_; ++column)
    {
        auto x = x0 + column;
        if (x >= width_)
            break;
        for (int row = 0; row < pixel_size_; ++row)
        {
            auto y = y0 + row;
            if (y >= height_)
                break;
            setPixel(buffer_, x, y, color);
        }
    }
}

} // namespace pixel_view
} // namespace cpprofiler
