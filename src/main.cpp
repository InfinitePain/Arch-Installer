#include "CMakeProjectConfig.h"
#include "Installer.h"

struct Args {
    std::vector<std::string> steps;
    bool debugMode = false;
};

Args parseArguments(int argc, const char* argv[]) {
    Args args;
    std::vector<std::string> cmdArgs(argv + 1, argv + argc);

    // Show help message
    if (std::find(cmdArgs.begin(), cmdArgs.end(), "-h") != cmdArgs.end()) {
        std::cout << "Arch Linux Installer\n"
                  << "Usage: [options]\n"
                  << "Options:\n"
                  << "  -h          Show this help message\n"
                  << "  -s [steps]  Specify installation steps (e.g., -s 1,2,3)\n"
                  << "  -d          Enable debug mode (dry run, step-by-step execution)\n"
                  << "\nThis program is a command-line installer for Arch Linux, "
                  << "written in C++ and using ncurses for the UI.\n"
                  << "Installation is divided into 3 steps:\n"
                  << "  Step 1: Set the console keyboard layout, update the system clock, and partition the disks\n"
                  << "  Step 2: Select the mirrors, and install the base packages\n"
                  << "  Step 3: Configure the system and install the boot loader\n"
                  << "In debug mode, no commands are run; instead, a dry run is performed. "
                  << "If run under a debugger, a debug break occurs after each dry run step.\n"
                  << "For more information, visit: www.github.com/InfinitePain/Arch-Installer\n";
        exit(0);
    }

    // Show version information
    if (std::find(cmdArgs.begin(), cmdArgs.end(), "-v") != cmdArgs.end()) {
        std::cout << "Arch Linux Installer Version "
                  << INSTALLER_VERSION_MAJOR << "."
                  << INSTALLER_VERSION_MINOR << "."
                  << INSTALLER_VERSION_PATCH << std::endl;
        exit(0);
    }

    auto findArg = [&cmdArgs](const std::string& flag) {
        return std::find(cmdArgs.begin(), cmdArgs.end(), flag);
    };

    if (findArg("-d") != cmdArgs.end()) {
        args.debugMode = true;
    }

    auto it = findArg("-s");
    if (it != cmdArgs.end() && std::next(it) != cmdArgs.end()) {
        std::stringstream ss(*std::next(it));
        std::string token;
        while (std::getline(ss, token, ',')) {
            args.steps.push_back(token);
        }
    } else {
        args.steps = {"1", "2", "3"};
    }

    return args;
}

int main(int argc, char const* argv[]) {
    Args parsedArgs = parseArguments(argc, argv);

    Installer installer;
    if (!installer.Init()) {
        std::cerr << "Failed to initialize installer" << std::endl;
        return 1;
    }

    // Array of functions to call for each step
    std::vector<std::function<void()>> stepsFunctions = {
        [&installer]() { installer.Step1(); },
        [&installer]() { installer.Step2(); },
        [&installer]() { installer.Step3(); }
    };

    if (parsedArgs.debugMode)
        installer.DebugMode();

    // Installer
    try {
        for (std::string step : parsedArgs.steps) {
            int stepNumber = std::stoi(step);
            if (stepNumber > 0 && stepNumber <= stepsFunctions.size()) {
                stepsFunctions[stepNumber - 1]();
            }
            else {
                throw std::invalid_argument("Invalid step");
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
