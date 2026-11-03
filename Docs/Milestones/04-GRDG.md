# Milestone 4 — Begin GRDG development

**Status:** not started. **Prerequisites:** stable allocation/lifetime contracts and native subsystem interfaces from the earlier milestones.

Establish the first executable slice of the Global Resource Dependency Graph: canonical typed resource identity, generation-safe handles, dependency metadata, one-writer publication, multiple-reader access and an explicitly tested reclamation strategy. The goal of this milestone is a reliable resource-ownership kernel, not an immediate conversion of every engine subsystem.

## Deliverables

1. Stable resource-key/handle API with resource types and generation validation.
2. Immutable published records, atomic publication and reader lifetime mechanism with documented memory ordering.
3. Allocation-domain accounting for resource payloads and derived provider representations.
4. Tests for handle reuse, concurrent publication, reader retirement, dependencies, invalid input and failed resource loads.

**Exit gate:** a source-complete, compiled and sanitizer-tested initial GRDG module with measured memory and update costs. Subsequent consumer migrations are planned separately.
