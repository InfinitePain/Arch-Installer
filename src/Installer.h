#ifndef INSTALLER_H_
#define INSTALLER_H_

#include "KeyEvent.h"
#include "Input.h"
#include "Renderer.h"
#include "Menu.h"
#include "CLI.h"

class Installer {
public:
    Installer() = default;
    ~Installer() = default;
    bool Init() {
        // Setup Input
        m_Input.Init();
        m_Input.SetCallback([](char* c) {
            std::unique_ptr<KeyEvent> event = std::make_unique<KeyEvent>(c);
            EVENT_PUSH(std::move(event));
            return true;
            });

        // Setup Renderer
        try {
            m_Renderer.Init();
            int maxy = getmaxy(stdscr);
            int maxx = getmaxx(stdscr);
            m_MainLayer = m_Renderer.CreateLayer(maxy, maxx, 0, 0);
            m_SubLayer = m_Renderer.CreateSubLayer(m_MainLayer, maxy - 2, maxy - 2, 1, 1);
            m_MainWindow = m_Renderer.GetWindowPtr(m_MainLayer);
            m_SubWindow = m_Renderer.GetWindowPtr(m_SubLayer);
        }
        catch (std::exception& e) {
            std::cout << e.what() << std::endl;
            return false;
        }

        m_DebuggerPresent = _IsDebuggerPresent();

        return true;
    }

    void DebugMode() {
        m_Debug = true;
    }

    void Step1() {
        try {
            _KBLayout();
            _SystemClock();
            _PartitionDisks();
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            _DebugStop();
            throw e;
        }
    }

    void Step2() {
        try {
            _SelectMirrors();
            _InstallPackages();
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            _DebugStop();
            throw e;
        }
    }

    void Step3() {
        try {
            _Chroot();
            _TimeZone();
            _Localization();
            _NetworkConfiguration();
            _Initramfs();
            _Accounts();
            _BootLoader();
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            _DebugStop();
            throw e;
        }
    }


private:
    bool _IsDebuggerPresent() {
        std::ifstream ifs("/proc/self/status");
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.find("TracerPid:") != std::string::npos) {
                return std::stoi(line.substr(line.find_last_of('\t') + 1)) != 0;
            }
        }
        return false;
    }

    void _RunCommand(const std::string& command, const std::string& args) {
        if (m_Debug) {
            m_Renderer.StopRenderer();
            std::cout << "\033[2J\033[1;1H" << std::flush; // Clean the screen
            std::cout << "Dry Run: " << command << " " << args << std::endl;
            _DebugStop();
        }
        else {
            CLI::RunCommand(command.c_str(), args.c_str());
        }
    }

    void _RunInteractiveCommand(const std::string& command, const std::string& args) {
        if (m_Debug) {
            m_Renderer.StopRenderer();
            std::cout << "\033[2J\033[1;1H" << std::flush; // Clean the screen
            std::cout << "Dry Run: " << command << " " << args << std::endl;
            _DebugStop();
        }
        else {
            m_Input.PauseInputHandler();
            CLI::RunInteractiveCommand(command.c_str(), args.c_str());
            m_Input.ResumeInputHandler();
        }
    }

    void _WriteToFile(const std::string& file, const std::string& content) {
        if (m_Debug) {
            m_Renderer.StopRenderer();
            std::cout << "\033[2J\033[1;1H" << std::flush; // Clean the screen
            std::cout << "Dry Run: " << file << ":\n" << content << std::endl;
            _DebugStop();
        }
        else {
            CLI::WriteToFile(file, content);
        }
    }

    void _DebugStop() {
        if (m_DebuggerPresent) {
            raise(SIGTRAP);
        }
        else {
            std::cout << "Press enter key to continue . . ." << std::endl;
            getchar();
        }
    }

    void _KBLayout() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        // Might throw std::bad_alloc cause of Menu::Init()
        std::string command;
        std::string args;
        std::string output;
        output = CLI::RunCommand("localectl", "list-keymaps");
        if (output.empty()) {
            throw std::runtime_error("Failed to get keyboard layouts");
        }
        Menu menu = Menu(m_MainWindow, m_SubWindow);
        menu.Init(output);

        while (true) {
            std::unique_ptr<KeyEvent> event = EVENT_POP();
            if (event != nullptr) {
                menu.OnEvent(*event.get());
            }
            m_Renderer.OnUpdate();
            if (menu.IsSelected()) {
                break;
            }
        }

        command = "loadkeys";
        args = menu.GetSelected();
        m_Keymap = args;
        _RunCommand(command, args);
    }

    void _SystemClock() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        // Might throw std::bad_alloc cause of Menu::Init()
        std::string command;
        std::string args;
        std::string output;
        output = CLI::RunCommand("timedatectl", "list-timezones");
        if (output.empty()) {
            throw std::runtime_error("Failed to get timezones");
        }
        Menu menu = Menu(m_MainWindow, m_SubWindow);
        menu.Init(output);

        while (true) {
            std::unique_ptr<KeyEvent> event = EVENT_POP();
            if (event != nullptr) {
                menu.OnEvent(*event.get());
            }
            m_Renderer.OnUpdate();
            if (menu.IsSelected()) {
                break;
            }
        }
        command = "timedatectl";
        args = "set-timezone " + menu.GetSelected();
        m_Timezone = menu.GetSelected();
        _RunCommand(command, args);
    }

    void _PartitionDisks() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        // Might throw std::bad_alloc cause of Menu::Init()
        std::string command;
        std::string args;
        std::string output;
        output = CLI::RunCommand("lsblk");
        if (output.empty()) {
            throw std::runtime_error("Failed to get disks");
        }
        Menu menu = Menu(m_MainWindow, m_SubWindow);
        menu.Init(output);

        while (true) {
            std::unique_ptr<KeyEvent> event = EVENT_POP();
            if (event != nullptr) {
                menu.OnEvent(*event.get());
            }
            m_Renderer.OnUpdate();
            if (menu.IsSelected()) {
                break;
            }
        }
        command = "cfdisk";

        args = "/dev/" + CLI::ExtractDiskOrPartitionName(menu.GetSelected());
        _RunInteractiveCommand(command, args);
        std::cout << "\033[2J\033[1;1H"; // Clean the screen
        std::cout << "Your currently in a shell inside the installer, you can run any command you want." << std::endl;
        std::cout << "Here you should format and mount the partitions you created." << std::endl;
        std::cout << "After you are done, type 'exit' to continue." << std::endl;
        command = "bash";
        args = "";
        _RunInteractiveCommand("bash", "");
    }

    void _SelectMirrors() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        _RunCommand("reflector", "--verbose --latest 5 --sort rate --save /etc/pacman.d/mirrorlist");
    }

    void _InstallPackages() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        // Might throw std::bad_alloc cause of Menu::Init()
        _RunCommand("pacstrap", "/mnt base linux linux-firmware linux-lts");
        _RunCommand("arch-chroot", "/mnt");
        std::ostringstream oss;
        oss << "NetworkManager\n" << "less\n" << "curl\n" << "base-devel\n";
        oss << "usbutils\n" << "reflector\n" << "wget\n" << "htop\n";
        oss << "git\n" << "networkmanager\n" << "networkmanager-openvpn\n";
        oss << "network-manager-applet\n" << "openvpn\n" << "pacman-contrib\n";
        oss << "neofetch\n" << "xorg-xrandr\n" << "spotify-launcher\n";
        oss << "libreoffice\n" << "flatpak\n" << "xdg-desktop-portal\n";
        oss << "okular\n" << "ntfs-3g\n" << "python-pip\n" << "python-pipx\n";
        oss << "xdg-utils\n" << "ddcutil\n" << "yakuake\n" << "gnome-calculator\n";
        oss << "gnome-text-editor\n" << "nautilus-share\n";
        oss << "nautilus\n" << "gvfs-smb\n";
        Menu menu = Menu(m_MainWindow, m_SubWindow);
        menu.Init(oss.str());
        menu.TogglableItems(true);
        mvwprintw(m_MainWindow, 0, 0, "Use space to remove the packages you don't want enter to continue");

        while (true) {
            std::unique_ptr<KeyEvent> event = EVENT_POP();
            if (event != nullptr) {
                menu.OnEvent(*event.get());
            }
            m_Renderer.OnUpdate();
            if (menu.IsSelected()) {
                break;
            }
        }

        std::string selected = menu.GetSelected();
        std::string args = oss.str();
        // Split the 'selected' string into individual items
        std::istringstream iss(selected);
        std::vector<std::string> itemsToRemove;
        std::string item;
        while (iss >> item) {
            itemsToRemove.push_back(item);
        }
        // Remove the packages that are selected
        for (auto& item : itemsToRemove) {
            args.erase(args.find(item), item.length() + 1);
        }
        // replace all newlines with spaces
        std::replace(args.begin(), args.end(), '\n', ' ');
        std::string command = "pacman";
        args = "-S " + args;
        _RunInteractiveCommand(command, args);
        _RunCommand("exit", "");
    }

    void _Chroot() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        _RunCommand("arch-chroot", "/mnt");
    }

    void _TimeZone() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        std::string command = "ln";
        std::string args = "-sf /usr/share/zoneinfo/" + m_Timezone + " /etc/localtime";
        _RunCommand(command, args);
        _RunCommand("hwclock", "--systohc");
    }

    void _Localization() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        std::string args;
        std::string command;
        m_Input.PauseInputHandler();
        std::cout << "/etc/locale.gen will be opened in nano, ";
        std::cout << "uncomment the locales you want to use and save the file." << std::endl;
        std::cout << "\rPress enter to continue." << std::endl;
        getchar();
        _RunInteractiveCommand("nano", "/etc/locale.gen");
        _RunCommand("locale-gen", "");
        m_Input.PauseInputHandler();
        std::cout << "Enter your locale (e.g. en_US.UTF-8): ";
        std::cin >> args;
        std::cout << "\n\r" << std::flush;
        command = "/etc/locale.conf";
        args = "LANG=" + args;
        _WriteToFile(command, args);
        command = "/etc/vconsole.conf";
        args = "KEYMAP=" + m_Keymap;
        _WriteToFile(command, args);
    }

    void _NetworkConfiguration() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        m_Input.PauseInputHandler();
        std::string command;
        std::string args;
        std::cout << "Enter your hostname: ";
        std::cin >> args;
        command = "/etc/hostname";
        _WriteToFile(command, args);
    }

    void _Initramfs() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        _RunCommand("mkinitcpio", "-P");
    }

    void _Accounts() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        std::string command;
        std::string args;
        std::string username;
        std::cout << "An interactive shell will with passwd command run for you to set the root password." << std::endl;
        std::cout << "Press enter to continue." << std::endl;
        getchar();
        _RunInteractiveCommand("passwd", "");
        m_Input.PauseInputHandler();
        std::cout << "Creating a user account." << std::endl;
        std::cout << "Enter your username: ";
        std::cin >> username;
        command = "useradd";
        args = "-m -G wheel " + args;
        _RunCommand(command, args);
        std::cout << "\nAn interactive shell will with passwd command run for you to set the user password." << std::endl;
        std::cout << "Press enter to continue." << std::endl;
        getchar();
        _RunInteractiveCommand("passwd", username);
    }

    void _BootLoader() {
        // Might throw std::runtime_error cause of CLI::RunCommand() or CLI::RunInteractiveCommand()
        std::string command;
        std::string args;
        std::string dir;
        _RunCommand("bootctl", "install");
        m_Input.PauseInputHandler();
        std::cout << "Configuring the boot loader." << std::endl;
        std::cout << "Each entry will be done automatically, than you will be dropped";
        std::cout << "into nano to edit to your liking." << std::endl;
        std::cout << "Default entry will be set to arch.conf." << std::endl;
        std::cout << "Enter the path where the boot partition is mounted (e.g. /boot): ";
        std::cin >> dir;
        command = dir + "/loader/loader.conf";
        std::ostringstream oss;
        oss << "default arch.conf\n";
        oss << "timeout 0\n";
        oss << "console-mode max\n";
        oss << "editor no\n";
        args = oss.str();
        _WriteToFile(command, args);
        _RunInteractiveCommand("nano", command);

        command = dir + "/loader/entries/arch.conf";
        oss.str("");
        oss << "title Arch Linux\n";
        oss << "linux /vmlinuz-linux\n";
        oss << "initrd /initramfs-linux.img\n";
        oss << "options root=\"LABEL=Arch OS\" rw quiet\n";
        args = oss.str();
        _WriteToFile(command, args);
        _RunInteractiveCommand("nano", command);

        command = dir + "/loader/entries/arch-lts.conf";
        oss.str("");
        oss << "title Arch Linux LTS\n";
        oss << "linux /vmlinuz-linux-lts\n";
        oss << "initrd /initramfs-linux-lts.img\n";
        oss << "options root=\"LABEL=Arch OS\" rw quiet\n";
        args = oss.str();
        _WriteToFile(command, args);
        _RunInteractiveCommand("nano", command);

        command = dir + "/loader/entries/arch-fallback.conf";
        oss.str("");
        oss << "title Arch Linux Fallback\n";
        oss << "linux /vmlinuz-linux\n";
        oss << "initrd /initramfs-linux-fallback.img\n";
        oss << "options root=\"LABEL=Arch OS\" rw quiet\n";
        args = oss.str();
        _WriteToFile(command, args);
        _RunInteractiveCommand("nano", command);

        command = dir + "/loader/entries/arch-lts-fallback.conf";
        oss.str("");
        oss << "title Arch Linux LTS Fallback\n";
        oss << "linux /vmlinuz-linux-lts\n";
        oss << "initrd /initramfs-linux-lts-fallback.img\n";
        oss << "options root=\"LABEL=Arch OS\" rw quiet\n";
        args = oss.str();
        _WriteToFile(command, args);
        _RunInteractiveCommand("nano", command);
    }

private:
    Renderer m_Renderer;
    WinHandle m_MainLayer;
    WinHandle m_SubLayer;
    WINDOW* m_MainWindow;
    WINDOW* m_SubWindow;
    InputHandler m_Input;
    std::string m_Keymap;
    std::string m_Timezone;
    bool m_DebuggerPresent = false;
    bool m_Debug = false;
};

#endif /*INSTALLER_H_*/
