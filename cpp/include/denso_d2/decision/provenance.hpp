#pragma once

#include <string>

namespace denso_d2::decision {

// Data-parameter source types, canonical enum from the data layer
// (LaTeX eq:srcset). Do not extend without
// a data-layer owner decision.
enum class SourceType {
    Synthetic,
    Observed,
    Estimated,
    Assumed,
    MentorConfirmed,
    SystemExport,
};

// Claim-status labels attached to modeling statements (LaTeX tab:status).
// GENERIC      structural property valid across factories
// SYNTHETIC    invented development value
// CANDIDATE    proposed structure awaiting Factory Tour validation
// OBSERVED     measured during the Factory Tour, with provenance
// MENTOR_CONFIRMED  validated by a DENSO mentor
// UNKNOWN      required information not yet available
enum class ClaimStatus {
    Generic,
    Synthetic,
    Candidate,
    Observed,
    MentorConfirmed,
    Unknown,
};

// Provenance record attached to parameters and action definitions.
// Mirrors the canonical tuple: value, unit, source, confidence,
// validation status, needs_mentor_validation.
struct Provenance {
    SourceType source_type;
    std::string source_note;
    bool needs_mentor_validation;
};

}  // namespace denso_d2::decision
