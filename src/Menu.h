#include <ncurses.h>
#include <menu.h>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <memory>
#include <system_error>
#include <cerrno>
#include <iostream>

#include "KeyEvent.h"

class Menu {
public:
    Menu(WINDOW* menuWin, WINDOW* menuSubWin) :
        m_MenuWin(menuWin), m_MenuSubWin(menuSubWin) {
        box(m_MenuWin, 0, 0);
    }
    ~Menu() = default;

    bool Init(const std::string& items) {
        try
        {
            // Parse the items string and create menu items
            std::istringstream stream(items);
            std::string line;
            while (std::getline(stream, line)) {
                m_MenuItemStrings.emplace_back(std::make_unique<char[]>(line.size() + 1));
                std::strcpy(m_MenuItemStrings.back().get(), line.c_str());
                ITEM* rawItem = new_item(m_MenuItemStrings.back().get(), nullptr);
                if (!rawItem)
                    throw std::bad_alloc();
                set_item_userptr(rawItem, (void*)m_MenuItemStrings.back().get());
                m_MenuItems.emplace_back(rawItem, _ItemDeleter());
            }

            // Create array of menu items for MENU
            ITEM** rawItems;
            rawItems = (ITEM**)calloc(m_MenuItems.size() + 1, sizeof(ITEM*));
            if (!rawItems)
                throw std::bad_alloc();

            // Create MENU
            m_MenuItemsArray.reset(rawItems);
            for (int i = 0; i < m_MenuItems.size(); ++i) {
                rawItems[i] = m_MenuItems[i].get();
            }
            rawItems[m_MenuItems.size()] = nullptr;
            MENU* rawMenu = new_menu(rawItems);
            if (!rawMenu)
                throw std::bad_alloc();
            m_Menu.reset(rawMenu);
            set_menu_win(rawMenu, m_MenuWin);
            set_menu_sub(rawMenu, m_MenuSubWin);
            set_menu_format(rawMenu, getmaxy(m_MenuSubWin), 1);
            _SetMenuOpts();
            post_menu(rawMenu);
        }
        catch (const std::exception& e)
        {
            std::ostringstream msg;
            msg << "Failed to create menu at line " << __LINE__ << " in function " << __FILE__;
            std::cerr << msg.str() << '\n';
            return false;
        }
        return true;
    }

    bool IsSelected() {
        if (m_selected) {
            m_selected = false;
            return true;
        }
        return false;
    }

    void OnEvent(KeyEvent& event) {
        post_menu(m_Menu.get());
        if (m_selected) {
            return;
        }
        char key[10];
        std::memcpy(key, event.GetKey(), 10);
        if (key[0] == '\033' && key[1] == '[' && key[2] == 'A') { // Up arrow
            menu_driver(m_Menu.get(), REQ_UP_ITEM);
        }
        else if (key[0] == '\033' && key[1] == '[' && key[2] == 'B') { // Down arrow
            menu_driver(m_Menu.get(), REQ_DOWN_ITEM);
        }
        else if (key[0] == '\033' && key[1] == '[' && key[2] == '5') { // Page up
            menu_driver(m_Menu.get(), REQ_SCR_UPAGE);
        }
        else if (key[0] == '\033' && key[1] == '[' && key[2] == '6') { // Page down
            menu_driver(m_Menu.get(), REQ_SCR_DPAGE);
        }
        else if (m_MenuOpts.m_Togglable && key[0] == ' ') // Space
        {
            menu_driver(m_Menu.get(), REQ_TOGGLE_ITEM);
        }
        else if (key[0] == '\n' || key[0] == '\r') { // Enter
            m_selected = true;
        }
    }

    std::string GetSelected() {
        if (!m_Menu.get()) {
            return "";
        }

        if (!m_MenuOpts.m_Togglable) {
            ITEM* cur = current_item(m_Menu.get());
            return item_name(cur);
        }
        else {
            std::ostringstream selected;
            ITEM** items = menu_items(m_Menu.get());
            for (int i = 0; i < item_count(m_Menu.get()); ++i) {
                if (item_value(items[i])) {
                    selected << item_name(items[i]) << " ";
                }
            }
            return selected.str();
        }
    }

    MENU* GetMenu() {
        return m_Menu.get();
    }

    void TogglableItems(bool togglable) {
        m_MenuOpts.m_Togglable = togglable;
        _SetMenuOpts();
    }

    void SetMenuMark(const std::string& mark) {
        m_MenuOpts.m_MenuMark = mark;
        _SetMenuOpts();
    }
private:
    // Custom deleter for Item smart pointers
    struct _ItemDeleter {
        void operator()(ITEM* item) const {
            if (item) {
                free_item(item);
            }
        }
    };
    // Custom deleter for Item Array smart pointers
    struct _ItemArrayDeleter {
        void operator()(ITEM** items) const {
            if (items) {
                free(items);
            }
        }
    };
    // Custom deleter for Menu smart pointers
    struct _MenuDeleter {
        void operator()(MENU* menu) const {
            if (menu) {
                unpost_menu(menu);
                free_menu(menu);
            }
        }
    };

    void _SetMenuOpts() {
        if (!m_Menu.get()) {
            return;
        }
        unpost_menu(m_Menu.get());
        if (m_MenuOpts.m_Togglable) {
            menu_opts_off(m_Menu.get(), O_ONEVALUE);
        }
        else {
            menu_opts_on(m_Menu.get(), O_ONEVALUE);
        }
        set_menu_mark(m_Menu.get(), m_MenuOpts.m_MenuMark.c_str());
        post_menu(m_Menu.get());
    }

private:
    WINDOW* m_MenuWin = nullptr;
    WINDOW* m_MenuSubWin = nullptr;
    std::vector<std::unique_ptr<char[]>> m_MenuItemStrings;
    std::vector<std::unique_ptr<ITEM, _ItemDeleter>> m_MenuItems;
    std::unique_ptr<ITEM*, _ItemArrayDeleter> m_MenuItemsArray = nullptr;
    std::unique_ptr<MENU, _MenuDeleter> m_Menu = nullptr;
    bool m_selected = false;
    struct MenuOpts {
        std::string m_MenuMark = " > ";
        bool m_Togglable = false;
    } m_MenuOpts;

};
