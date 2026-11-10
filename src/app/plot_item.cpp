#include "app/plot_item.h"
#include <QImage>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGSimpleTextureNode>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <vector>

namespace {
QSGGeometryNode* geometryNode(QColor color, unsigned mode) {
    auto* node = new QSGGeometryNode;
    auto* data = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    data->setDrawingMode(mode); data->setLineWidth(1);
    data->setVertexDataPattern(QSGGeometry::DynamicPattern);
    node->setGeometry(data); node->setFlag(QSGNode::OwnsGeometry);
    auto* material = new QSGFlatColorMaterial; material->setColor(color);
    node->setMaterial(material); node->setFlag(QSGNode::OwnsMaterial);
    return node;
}
void points(QSGGeometryNode* node, const std::vector<QPointF>& values) {
    auto* data = node->geometry();
    if (data->vertexCount() != static_cast<int>(values.size())) data->allocate(static_cast<int>(values.size()));
    auto* vertices = data->vertexDataAsPoint2D();
    for (const auto& point : values) { vertices->set(static_cast<float>(point.x()), static_cast<float>(point.y())); ++vertices; }
    data->markVertexDataDirty(); node->markDirty(QSGNode::DirtyGeometry);
}
QColor heat(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    const QColor stops[]{QColor("#101321"), QColor("#423d92"), QColor("#9c54bb"), QColor("#f38172"), QColor("#fff0b4")};
    const auto segment = std::min(3, static_cast<int>(value * 4));
    const auto mix = value * 4 - segment;
    const auto a = stops[segment], b = stops[segment + 1];
    return QColor::fromRgbF(a.redF() + mix * (b.redF() - a.redF()), a.greenF() + mix * (b.greenF() - a.greenF()), a.blueF() + mix * (b.blueF() - a.blueF()));
}
struct Tile { QSGSimpleTextureNode* node = nullptr; std::uint64_t first = 0, last = 0; };
class PlotNode : public QSGNode {
public:
    PlotNode() {
        imageLayer = new QSGNode; appendChildNode(imageLayer);
        grid = geometryNode(QColor("#263242"), QSGGeometry::DrawLines); appendChildNode(grid);
        left = geometryNode(QColor("#74e3c7"), QSGGeometry::DrawLines); appendChildNode(left);
        right = geometryNode(QColor("#7ba9f7"), QSGGeometry::DrawLines); appendChildNode(right);
        reference = geometryNode(QColor("#f4bb7b"), QSGGeometry::DrawLineStrip); appendChildNode(reference);
    }
    void clearTiles() {
        for (auto& [key, tile] : tiles) { imageLayer->removeChildNode(tile.node); delete tile.node; }
        tiles.clear();
    }
    QSGNode* imageLayer;
    QSGGeometryNode *grid, *left, *right, *reference;
    std::map<std::uint64_t, Tile> tiles;
    QString analyzer;
    double width = -1, height = -1, floor = 0, gain = 0, slope = 0, history = 0;
    std::uint64_t segment = 0, sequence = 0;
    int rate = 0;
    bool active = false;
    std::shared_ptr<const daa::ReferenceResult> previousReference;
};
}

PlotItem::PlotItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents);
    connect(this, &PlotItem::settingsChanged, this, [this] { update(); });
}
void PlotItem::geometryChange(const QRectF& next, const QRectF& previous) { QQuickItem::geometryChange(next, previous); update(); }
void PlotItem::setEngine(AudioEngine* value) {
    if (engine_ == value) return;
    if (engine_) disconnect(engine_, nullptr, this, nullptr);
    engine_ = value;
    if (engine_) {
        connect(engine_, &AudioEngine::frameChanged, this, [this] {
            if (isVisible() && window() && window()->isVisible() && window()->visibility() != QWindow::Minimized && !held_) update();
        });
        connect(engine_, &AudioEngine::referenceChanged, this, [this] { update(); });
    }
    emit settingsChanged();
}
void PlotItem::setHeld(bool held) {
    if (held_ == held) return;
    held_ = held; heldSnapshot_ = held && engine_ ? engine_->snapshot() : nullptr; emit settingsChanged();
}
QSGNode* PlotItem::updatePaintNode(QSGNode* previous, UpdatePaintNodeData*) {
    auto* root = static_cast<PlotNode*>(previous);
    if (!root) root = new PlotNode;
    if (!engine_ || width() <= 0 || height() <= 0) return root;
    const auto snapshot = held_ ? heldSnapshot_ : engine_->snapshot();
    if (!snapshot) return root;
    const auto w = width(), h = height();
    const bool active = held_ || engine_->active();
    const bool style = root->analyzer != analyzer_ || root->width != w || root->height != h ||
        root->floor != floor_ || root->gain != gain_ || root->slope != slope_ || root->history != history_ ||
        root->segment != snapshot->segments || root->rate != static_cast<int>(snapshot->sampleRate) || root->active != active;
    const bool spectrumChanged = style || root->sequence != snapshot->spectrogramSequence;
    if (style) {
        root->clearTiles();
        std::vector<QPointF> grid;
        if (analyzer_ == "waveform") {
            for (const auto y : {h * 0.25, h * 0.75}) { grid.emplace_back(0, y); grid.emplace_back(w, y); }
            for (int i = 1; i < 10; ++i) { grid.emplace_back(w * i / 10, 0); grid.emplace_back(w * i / 10, h); }
        } else {
            if (analyzer_ == "spectrum") for (int db = -20; db > floor_; db -= 20) {
                const auto y = h * db / floor_; grid.emplace_back(0, y); grid.emplace_back(w, y);
            }
            for (const auto hz : {100.0, 1000.0, 10000.0}) {
                const auto maximum = std::min(20000.0, snapshot->sampleRate / 2.0);
                if (hz >= maximum) continue;
                const auto x = std::log(hz / 20) / std::log(maximum / 20) * w;
                grid.emplace_back(x, 0); grid.emplace_back(x, h);
            }
        }
        points(root->grid, grid);
        points(root->left, {}); points(root->right, {}); points(root->reference, {});
    }
    if (analyzer_ == "waveform" && active) {
        const auto& history = snapshot->waveform;
        const auto count = std::min(history.size(), static_cast<std::size_t>(history_ * daa::Analysis::envelopeRate));
        const auto total = std::max(1.0, history_ * daa::Analysis::envelopeRate);
        const double gain = std::pow(10, gain_ / 20);
        for (int channel = 0; channel < 2; ++channel) {
            std::vector<QPointF> waveform; waveform.reserve(static_cast<std::size_t>(w) * 2);
            const auto center = h * (channel == 0 ? 0.25 : 0.75);
            for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(w); ++pixel) {
                const auto start = static_cast<long long>(std::floor(double(pixel) / w * total - (total - count)));
                const auto end = static_cast<long long>(std::ceil(double(pixel + 1) / w * total - (total - count)));
                if (end <= 0 || start >= static_cast<long long>(count)) continue;
                float low = 1e10f, high = -1e10f;
                for (auto i = std::max(0LL, start); i < std::min(static_cast<long long>(count), end); ++i) {
                    const auto& envelope = history[history.size() - count + static_cast<std::size_t>(i)];
                    low = std::min(low, channel == 0 ? envelope.minLeft : envelope.minRight);
                    high = std::max(high, channel == 0 ? envelope.maxLeft : envelope.maxRight);
                }
                waveform.emplace_back(pixel, center - std::clamp(double(high) * gain, -1.0, 1.0) * h * 0.21);
                waveform.emplace_back(pixel, center - std::clamp(double(low) * gain, -1.0, 1.0) * h * 0.21);
            }
            points(channel == 0 ? root->left : root->right, waveform);
        }
    } else if (analyzer_ == "spectrum") {
        const auto curve = [&](QSGGeometryNode* node, const std::vector<float>& spectrum, double rate) {
            const auto columns = std::clamp(static_cast<int>(w), 64, 2048);
            const auto data = daa::logSpectrum(spectrum, rate, daa::Analysis::fftSize, columns);
            std::vector<QPointF> curve; curve.reserve(data.size());
            const auto maximum = std::min(20000.0, rate / 2);
            for (std::size_t i = 0; i < data.size(); ++i) {
                const auto hz = 20 * std::pow(maximum / 20, (double(i) + 0.5) / columns);
                const auto db = data[i] + gain_ + slope_ * std::log2(hz / 1000);
                curve.emplace_back(double(i) / (columns - 1) * w, std::clamp(db / floor_, 0.0, 1.0) * h);
            }
            node->geometry()->setDrawingMode(QSGGeometry::DrawLineStrip); points(node, curve);
        };
        if (spectrumChanged && active) curve(root->left, snapshot->spectrum, snapshot->sampleRate);
        const auto reference = showReference_ ? engine_->reference() : nullptr;
        if (style || root->previousReference != reference) {
            if (reference && reference->sampleRate == snapshot->sampleRate) curve(root->reference, reference->spectrum, reference->sampleRate);
            else points(root->reference, {});
            root->previousReference = reference;
        }
    } else if (analyzer_ == "spectrogram" && active && spectrumChanged && snapshot->spectrogramRows > 0) {
        // Public Qt APIs only: retain completed 16-row tiles and upload just the
        // newest/oldest partial tiles. No full-history texture rebuild per frame.
        const auto wanted = std::max(1, static_cast<int>(history_ * snapshot->sampleRate / (daa::Analysis::fftSize / 4)));
        const auto count = std::min(snapshot->spectrogramRows, wanted);
        const auto last = snapshot->spectrogramSequence - 1;
        const auto first = last + 1 - count;
        const auto firstStored = snapshot->spectrogramSequence - snapshot->spectrogramRows;
        for (auto it = root->tiles.begin(); it != root->tiles.end();) {
            if (it->first < first / 16 || it->first > last / 16) {
                root->imageLayer->removeChildNode(it->second.node); delete it->second.node; it = root->tiles.erase(it);
            } else ++it;
        }
        std::array<QRgb, 256> palette;
        for (int i = 0; i < 256; ++i) palette[i] = heat(float(i) / 255).rgb();
        for (auto group = first / 16; group <= last / 16; ++group) {
            const auto begin = std::max(first, group * 16), end = std::min(last + 1, (group + 1) * 16);
            auto& tile = root->tiles[group];
            if (!tile.node || tile.first != begin || tile.last != end) {
                if (tile.node) { root->imageLayer->removeChildNode(tile.node); delete tile.node; }
                QImage image(snapshot->spectrogramColumns, static_cast<int>(end - begin), QImage::Format_RGB32);
                for (int y = 0; y < image.height(); ++y) {
                    auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                    for (int x = 0; x < image.width(); ++x) {
                        const auto db = snapshot->spectrogram[(begin - firstStored + y) * image.width() + x] + gain_;
                        const auto value = std::clamp((db - floor_) / -floor_, 0.0, 1.0);
                        line[x] = palette[static_cast<std::size_t>(value * 255)];
                    }
                }
                tile.node = new QSGSimpleTextureNode;
                tile.node->setTexture(window()->createTextureFromImage(image)); tile.node->setOwnsTexture(true);
                tile.node->setFiltering(QSGTexture::Linear); root->imageLayer->appendChildNode(tile.node);
                tile.first = begin; tile.last = end;
            }
            const auto y = (double(begin) - double(last + 1) + wanted) / wanted * h;
            tile.node->setRect(0, y, w, double(end - begin) / wanted * h);
        }
    }
    root->analyzer = analyzer_; root->width = w; root->height = h; root->floor = floor_; root->gain = gain_;
    root->slope = slope_; root->history = history_; root->segment = snapshot->segments;
    root->sequence = snapshot->spectrogramSequence; root->rate = static_cast<int>(snapshot->sampleRate); root->active = active;
    return root;
}
