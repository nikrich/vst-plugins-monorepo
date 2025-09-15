#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::UnitTestRunner r;
    r.runAllTests();
    return 0;
}

