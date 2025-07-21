# Implementation Plan

- [x] 1. Add core C infrastructure for query_by_key command





  - Add new runner mode constant `OLAF_RUNNER_MODE_QUERY_BY_KEY` to olaf_runner.h
  - Add command recognition logic in main() function of olaf.c
  - Create basic function signature for `olaf_query_by_key()`
  - _Requirements: 1.1, 3.1_

- [x] 2. Implement key parsing and validation functions


  - [x] 2.1 Create key parsing utility functions


    - Write function to parse hexadecimal keys (with and without 0x prefix)
    - Write function to parse decimal keys
    - Write function to validate key ranges (uint64_t bounds)
    - Create unit tests for key parsing functions
    - _Requirements: 4.1, 4.2, 4.3, 4.4_

  - [x] 2.2 Implement input argument processing


    - Write function to process command line key arguments
    - Write function to read keys from file input
    - Add error handling for invalid formats and missing files
    - Create unit tests for argument processing
    - _Requirements: 3.2, 3.3, 4.1, 4.2_

- [x] 3. Implement database query functionality

  - [x] 3.1 Create single key lookup function

    - Write function to query database for single key using olaf_db_find()
    - Handle case where key has no matches (empty result)
    - Handle case where key has multiple matches (hash collisions)
    - Create unit tests for single key queries
    - _Requirements: 1.1, 1.2, 1.3, 1.4_

  - [x] 3.2 Implement batch key processing

    - Write function to process multiple keys efficiently
    - Maintain performance for large key sets
    - Group results by input key for clear output
    - Create unit tests for batch processing
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [x] 4. Implement result formatting and output

  - [x] 4.1 Create basic result formatting

    - Write function to format single key results
    - Write function to format multiple key results
    - Implement consistent output format with existing query commands
    - Create unit tests for result formatting
    - _Requirements: 3.4, 5.1, 5.2_

  - [x] 4.2 Add metadata enrichment functionality


    - Write function to lookup audio metadata using olaf_db_find_meta_data()
    - Implement verbose output mode with file paths and duration
    - Add optional metadata display based on availability
    - Create unit tests for metadata enrichment
    - _Requirements: 5.3, 5.4_

- [x] 5. Complete C core implementation

  - Implement main olaf_query_by_key() function integrating all components
  - Add proper error handling and user feedback
  - Ensure memory management and resource cleanup
  - Add integration tests with actual database
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 2.4_

- [x] 6. Add Ruby CLI integration

  - [x] 6.1 Implement Ruby command structure


    - Add query_by_key command to commands hash in olaf.rb
    - Write query_by_key() function to handle CLI arguments
    - Add command line option parsing (--format, --verbose)
    - Create unit tests for Ruby CLI integration
    - _Requirements: 3.1, 3.2, 3.3_

  - [x] 6.2 Implement Ruby key processing helpers

    - Write Ruby functions for key format validation
    - Write Ruby functions for file input processing
    - Add error handling and user-friendly messages
    - Create unit tests for Ruby helper functions
    - _Requirements: 3.2, 3.3, 4.1, 4.2, 4.3, 4.4_

- [x] 7. Add comprehensive error handling

  - Implement error handling for invalid key formats in C core
  - Add file not found and access error handling
  - Implement database error handling using existing patterns
  - Add clear error messages following Olaf conventions
  - _Requirements: 4.3, 4.4, 3.5_

- [x] 8. Create integration tests

  - [x] 8.1 Test with populated database

    - Create test database with known fingerprint keys
    - Write tests for successful key lookups
    - Write tests for keys with multiple matches
    - Write tests for keys with no matches
    - _Requirements: 1.1, 1.2, 1.3, 1.4_

  - [x] 8.2 Test CLI functionality end-to-end

    - Test command line key input
    - Test file-based key input
    - Test various output formats
    - Test error conditions and edge cases
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_

- [x] 9. Performance testing and optimization

  - Write performance tests for single key queries
  - Write performance tests for batch key operations
  - Compare performance with existing query operations
  - Optimize memory usage for large key sets
  - _Requirements: 2.4_

- [x] 10. Documentation and help integration



  - Update help text in olaf.c for new command
  - Add usage examples to Ruby CLI help
  - Create documentation for key formats and usage
  - Add command to help output in print_help() function
  - _Requirements: 3.1, 3.4, 3.5_