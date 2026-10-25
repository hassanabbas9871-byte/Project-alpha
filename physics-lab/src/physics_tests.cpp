// This executable deliberately has no window and links no graphics library.
// It asks the very same simulator used by the playground to test its rules.
#include "Simulation.hpp"

int main() {
    return flux::runPhysicsChecks() ? 0 : 1;
}
