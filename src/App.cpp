#include "App.h"

#include <utility>

namespace
{
constexpr int initialWindowWidth = 2020;
constexpr int initialWindowHeight = 1460;
}

const juce::String GeilalizerApplication::getApplicationName()
{
    return JUCE_APPLICATION_NAME_STRING;
}

const juce::String GeilalizerApplication::getApplicationVersion()
{
    return JUCE_APPLICATION_VERSION_STRING;
}

bool GeilalizerApplication::moreThanOneInstanceAllowed()
{
    return true;
}

void GeilalizerApplication::initialise(const juce::String&)
{
    mainWindow = std::make_unique<MainWindow>(getApplicationName());
}

void GeilalizerApplication::shutdown()
{
    mainWindow = nullptr;
}

void GeilalizerApplication::systemRequestedQuit()
{
    quit();
}

void GeilalizerApplication::anotherInstanceStarted(const juce::String&)
{
}

GeilalizerApplication::MainWindow::MainWindow(juce::String name)
    : DocumentWindow(std::move(name), juce::Colours::black, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setResizeLimits(1100, 760, 2400, 2400);
    setContentOwned(new MainComponent(), true);
    centreWithSize(initialWindowWidth, initialWindowHeight);
    setVisible(true);
}

void GeilalizerApplication::MainWindow::closeButtonPressed()
{
    JUCEApplication::getInstance()->systemRequestedQuit();
}

START_JUCE_APPLICATION(GeilalizerApplication)
