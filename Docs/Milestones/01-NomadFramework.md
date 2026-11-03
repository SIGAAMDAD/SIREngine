# Milestone 1 - Port NomadFramework C++ systems

**Status:** planned.

Port selected reusable C/C++ framework services into SIREngine-owned core libraries. The supplied `nframework.zip` and the existing NomadFramework source history are technical inputs for this port; the source to be integrated must be reconciled with those originals before editing. The engine repository contains the selected first-party implementations only after they are reviewed and migrated.

## Deliverables

1. Source, license and API inventory of the selected framework services, with exact revision/archival provenance.
2. Native C++17 implementations and CMake targets for platform utilities, memory domains, logging, event dispatch, filesystem, console and configuration systems where supported by the real source.
3. Allocation ownership, initialization order and shutdown handling suitable for subsequent engine systems.
4. Windows/Linux compilation, source formatting, link and tests for each dependency-closed port.

**Exit criteria:** imported services are built and exercised as first-party SIREngine components with verified interfaces, source lineage, and resource behavior.
