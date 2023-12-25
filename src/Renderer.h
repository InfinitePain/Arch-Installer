#ifndef RENDERER_H_
#define RENDERER_H_

#include <ncurses.h>
#include <vector>
#include <algorithm>
#include <memory>

using WinHandle = int;

struct LayerProp {
    WINDOW* layer;
    int order;
    int height;
    int width;
    int starty;
    int startx;
    // childs cant have childs
    WinHandle parent = -1;
    WinHandle child = -1;

    LayerProp(int height, int width, int starty, int startx) :
        height(height), width(width), starty(starty), startx(startx) {}
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer() {
        if (m_Layers.size() > 0) {
            for (auto& layer : m_Layers) {
                delwin(layer.layer);
            }
        }
        endwin();
    }
    void Init() {
        initscr();
        curs_set(0);
    }

    WINDOW* GetWindowPtr(WinHandle handle) {
        if (handle < 0 || handle >= m_Layers.size()) {
            return nullptr;
        }
        return m_Layers[handle].layer;
    }

    void ChangeLayerOrder(WinHandle handle, unsigned int newOrder) {
        if (m_Layers.size() == 1) {
            return;
        }
        // Clamping the new order within the bounds of the order vector
        newOrder = std::min(newOrder, static_cast<unsigned int>(m_OrderVector.size()));
        newOrder = newOrder == 0 ? 1 : newOrder;

        // If the new order is the same as the current order, do nothing
        if (newOrder == m_Layers[handle].order) {
            return; // No change in order
        }

        // Find the current position of the layer in the order vector
        auto it = std::find(m_OrderVector.begin(), m_OrderVector.end(), handle);
        if (it == m_OrderVector.end()) {
            return; // Handle not found
        }

        // Remove the handle from its current position
        m_OrderVector.erase(it);

        // Insert the handle at the new position
        auto newIt = m_OrderVector.begin() + (newOrder - 1);
        m_OrderVector.insert(newIt, handle);

        // Update the order in the Layer object
        for (int i = 0; i < m_OrderVector.size(); ++i) {
            m_Layers[m_OrderVector[i]].order = i + 1;
        }
    }

    void OnUpdate() {
        for (auto& index : m_OrderVector) {
            wrefresh(m_Layers[index].layer);
        }
    }

    WinHandle CreateLayer(int height, int width, int starty, int startx) {
        LayerProp layer(height, width, starty, startx);
        layer.layer = newwin(height, width, starty, startx);
        if (layer.layer == nullptr) throw std::bad_alloc();
        layer.order = m_Layers.size() + 1;
        m_Layers.push_back(layer);
        m_OrderVector.push_back(m_Layers.size() - 1);
        return m_Layers.size() - 1;
    }

    WinHandle CreateSubLayer(WinHandle parent, int height, int width, int starty, int startx) {
        if (parent < 0 || parent >= m_Layers.size()) {
            return -1;
        }
        if (m_Layers[parent].child != -1 || m_Layers[parent].parent != -1) {
            return -1;
        }
        LayerProp layer(height, width, starty, startx);
        layer.layer = derwin(m_Layers[parent].layer, height, width, starty, startx);
        if (layer.layer == nullptr) throw std::bad_alloc();
        layer.order = m_Layers.size() + 1;
        layer.parent = parent;
        m_Layers[parent].child = m_Layers.size();
        m_Layers.push_back(layer);
        m_OrderVector.push_back(m_Layers.size() - 1);
        return m_Layers.size() - 1;
    }

    void DestroyLayer(WinHandle handle) {
        if (m_Layers.size() == 1) {
            return;
        }
        if (m_Layers[handle].child != -1) {
            DestroyLayer(m_Layers[handle].child);
        }
        if (m_Layers[handle].parent != -1) {
            m_Layers[m_Layers[handle].parent].child = -1;
        }
        delwin(m_Layers[handle].layer);
        m_Layers.erase(m_Layers.begin() + handle);
        m_OrderVector.erase(std::find(m_OrderVector.begin(), m_OrderVector.end(), handle));
    }

    inline int GetLayerOrder(WinHandle handle) {
        return m_Layers[handle].order;
    }

    inline void StopRenderer() {
        m_Running = false;
        endwin();
    }

private:
    std::vector<LayerProp> m_Layers;
    std::vector<WinHandle> m_OrderVector;
    bool m_Running = true;
};

#endif /*RENDERER_H_*/
