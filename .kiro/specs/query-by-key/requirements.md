# Requirements Document

## Introduction

This feature adds the ability to query the Olaf acoustic fingerprinting database using existing fingerprint keys that are already stored in the database, rather than requiring audio input. This enables direct key-based lookups for scenarios where fingerprint hashes are known but the original audio is not available or when performing database analysis and debugging operations.

## Requirements

### Requirement 1

**User Story:** As a developer using Olaf, I want to query the database using existing fingerprint keys, so that I can find matches without needing the original audio files.

#### Acceptance Criteria

1. WHEN a user provides a fingerprint key as input THEN the system SHALL search the database for that specific key
2. WHEN a matching key is found THEN the system SHALL return all associated values (audio identifier and timestamp information)
3. WHEN no matching key is found THEN the system SHALL return an empty result set
4. WHEN multiple values exist for the same key THEN the system SHALL return all matching values

### Requirement 2

**User Story:** As a system administrator, I want to query multiple keys at once, so that I can efficiently perform batch lookups for analysis or debugging.

#### Acceptance Criteria

1. WHEN a user provides multiple fingerprint keys THEN the system SHALL process all keys in a single operation
2. WHEN processing multiple keys THEN the system SHALL return results grouped by input key
3. WHEN some keys match and others don't THEN the system SHALL return results for matching keys and indicate which keys had no matches
4. WHEN processing large batches THEN the system SHALL maintain reasonable performance

### Requirement 3

**User Story:** As a developer, I want the query_by_key functionality to integrate with the existing CLI interface, so that I can use it consistently with other Olaf commands.

#### Acceptance Criteria

1. WHEN using the CLI THEN the system SHALL provide a `query_by_key` subcommand
2. WHEN using query_by_key THEN the system SHALL accept keys as command line arguments
3. WHEN using query_by_key THEN the system SHALL accept keys from a file input
4. WHEN displaying results THEN the system SHALL use a format consistent with existing query output
5. WHEN encountering errors THEN the system SHALL provide clear error messages following existing patterns

### Requirement 4

**User Story:** As a developer, I want to specify key input formats, so that I can work with keys in different representations (hexadecimal, decimal).

#### Acceptance Criteria

1. WHEN providing keys THEN the system SHALL accept hexadecimal format (0x prefix or without)
2. WHEN providing keys THEN the system SHALL accept decimal format
3. WHEN providing invalid key formats THEN the system SHALL return clear error messages
4. WHEN keys are out of valid range THEN the system SHALL return appropriate error messages

### Requirement 5

**User Story:** As a developer, I want the query_by_key results to include metadata about matches, so that I can understand the context of the fingerprint matches.

#### Acceptance Criteria

1. WHEN a key matches THEN the system SHALL return the audio identifier associated with the fingerprint
2. WHEN a key matches THEN the system SHALL return the timestamp information
3. WHEN metadata is available for the audio identifier THEN the system SHALL optionally include file path and duration information
4. WHEN verbose output is requested THEN the system SHALL include additional debugging information