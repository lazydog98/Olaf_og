# Design Document

## Overview

The query_by_key feature extends Olaf's existing database query capabilities to allow direct lookup of fingerprint keys without requiring audio input. This feature leverages the existing LMDB-based database infrastructure and follows the established patterns in the Olaf codebase for command handling and result formatting.

The implementation will add a new command mode to the existing C core (`OLAF_RUNNER_MODE_QUERY_BY_KEY`) and integrate it with the Ruby CLI interface as a new subcommand. The feature will reuse existing database query functions while providing a new input mechanism for direct key specification.

## Architecture

### Core Components

The implementation follows Olaf's existing architecture pattern:

1. **C Core Extension**: Add query_by_key functionality to the existing `olaf.c` main program
2. **Database Layer**: Utilize existing `olaf_db_find()` and related functions from `olaf_db.h`
3. **Ruby CLI Integration**: Add new command to the Ruby script with key parsing and formatting
4. **Result Processing**: Reuse existing result formatting patterns for consistency

### Data Flow

```
Input Keys (CLI) → Key Parsing → Database Query → Result Formatting → Output
```

1. User provides keys via command line arguments or file input
2. Ruby script parses and validates key formats (hex/decimal)
3. Keys are passed to C core for database lookup
4. Database returns matching values (audio_id + timestamp)
5. Results are formatted and optionally enriched with metadata
6. Output is displayed in consistent format

## Components and Interfaces

### C Core Changes

#### New Runner Mode
```c
#define OLAF_RUNNER_MODE_QUERY_BY_KEY 7654
```

#### Main Function Extension
Add new command handling in `main()` function:
```c
else if(strcmp(command,"query_by_key") == 0){
    runner_mode = OLAF_RUNNER_MODE_QUERY_BY_KEY;
}
```

#### Key Processing Function
New function to handle direct key queries:
```c
int olaf_query_by_key(int argc, const char* argv[])
```

This function will:
- Parse command line arguments for keys
- Convert string representations to uint64_t values
- Call existing `olaf_db_find()` for each key
- Format and output results

### Ruby CLI Integration

#### New Command Definition
Add to the `commands` hash in `olaf.rb`:
```ruby
"query_by_key" => {
  :description => "Query the database using existing fingerprint keys",
  :help => "[--format hex|dec] [--verbose] keys_or_file...",
  :needs_audio_files => false,
  :lambda => -> { query_by_key }
}
```

#### Key Parsing Functions
```ruby
def parse_key(key_string, format)
def query_by_key_from_file(filename)
def query_by_key_from_args(keys)
```

### Database Interface

The implementation will use existing database functions:
- `olaf_db_find()` - for key range queries (single key: start_key == stop_key)
- `olaf_db_find_meta_data()` - for optional metadata enrichment
- `olaf_db_string_hash()` - for any string-to-key conversions if needed

## Data Models

### Input Key Formats

**Hexadecimal Format:**
- With prefix: `0x1234567890ABCDEF`
- Without prefix: `1234567890ABCDEF`

**Decimal Format:**
- Standard: `1311768467294899695`

### Output Format

**Basic Output:**
```
Key: 0x1234567890ABCDEF
  Match: audio_id=12345, timestamp=67890
  Match: audio_id=12346, timestamp=67891
```

**Verbose Output (with metadata):**
```
Key: 0x1234567890ABCDEF (1311768467294899695)
  Match: audio_id=12345, timestamp=67890, file="/path/to/audio.mp3", duration=180.5s
  Match: audio_id=12346, timestamp=67891, file="/path/to/other.mp3", duration=240.2s
```

**CSV Output (for scripting):**
```
key,audio_id,timestamp,file_path,duration
0x1234567890ABCDEF,12345,67890,/path/to/audio.mp3,180.5
0x1234567890ABCDEF,12346,67891,/path/to/other.mp3,240.2
```

### File Input Format

Text file with one key per line:
```
0x1234567890ABCDEF
1311768467294899695
0xABCDEF1234567890
```

## Error Handling

### Input Validation
- **Invalid key format**: Clear error message with expected format examples
- **Key out of range**: Validate uint64_t bounds
- **File not found**: Standard file error handling
- **Empty input**: Appropriate message for no keys provided

### Database Errors
- **Database not accessible**: Reuse existing database error handling
- **No matches found**: Clear indication of no results (not an error)
- **Database corruption**: Leverage existing LMDB error handling

### Error Message Format
Follow existing Olaf patterns:
```
Error: Invalid key format '0xGHIJKL'. Expected hexadecimal (0x1234...) or decimal (1234...)
Error: Key '999999999999999999999' exceeds maximum value for 64-bit integer
Error: Could not find file 'keys.txt'
```

## Testing Strategy

### Unit Tests
1. **Key Parsing Tests**
   - Valid hexadecimal formats (with/without 0x prefix)
   - Valid decimal formats
   - Invalid formats and edge cases
   - Boundary values (0, UINT64_MAX)

2. **Database Query Tests**
   - Single key lookup with matches
   - Single key lookup without matches
   - Multiple key lookups
   - Key collision scenarios

3. **Output Formatting Tests**
   - Basic output format
   - Verbose output with metadata
   - CSV output format
   - Empty result handling

### Integration Tests
1. **CLI Integration**
   - Command line argument parsing
   - File input processing
   - Error handling and user feedback
   - Thread safety (if applicable)

2. **Database Integration**
   - Query against populated database
   - Query against empty database
   - Large batch queries
   - Performance with existing database sizes

### Test Data Setup
- Create test database with known fingerprint keys
- Prepare test files with various key formats
- Include edge cases and error conditions
- Test with realistic database sizes

### Performance Testing
- Measure query time for single keys
- Measure query time for batch operations
- Compare performance with existing query operations
- Memory usage analysis for large key sets

## Implementation Notes

### Reuse Existing Infrastructure
- Leverage existing `Olaf_Runner` structure for consistency
- Use existing database connection and transaction handling
- Follow established error handling patterns
- Maintain compatibility with existing configuration system

### Memory Management
- Efficient handling of large key lists
- Proper cleanup of allocated resources
- Consider streaming for very large input files

### Thread Safety
- Ensure compatibility with existing database locking
- Consider multi-threading for large batch operations
- Maintain consistency with existing query operations

### Backward Compatibility
- No changes to existing functionality
- New command does not affect existing database structure
- Maintains existing output formats where applicable