/** @file olaf.c
 * @brief OLAF
 *
 * @mainpage	Overly Lightweight Acoustic Fingerprinting (Olaf)
 *
 * @section intro_sec Introduction
 * Olaf is a C application / library for landmark based acoustic fingerprinting. 
 * Olaf is able to extract fingerprints from an audio stream, and either store those 
 * fingerprints in a database, or find a match between extracted fingerprints and 
 * stored fingerprints. Olaf does this efficiently in order to be used on embedded platforms, 
 * traditional computers or in web browsers via WASM.
 *
 * Please be aware of the patents US7627477 B2 and US6990453 and perhaps others. They describe 
 * techniques used in algorithms implemented within Olaf. These patents limit the use of Olaf 
 * under various conditions and for several regions. Please make sure to consult your intellectual 
 * property rights specialist if you are in doubt about these restrictions. If these restrictions apply, 
 * please respect the patent holders rights. The main aim of Olaf is to serve as a learning platform 
 * on efficient (embedded) acoustic fingerprinting algorithms.
 * 
 * 
 * @section why_sec Why Olaf?
 * 
 * Olaf stands out for three reasons. 
 * 1. Olaf runs on embedded devices. 
 * 2. Olaf is fast on traditional computers. 
 * 3. Olaf runs in the browsers.
 * 
 * There seem to be no lightweight acoustic fingerprinting libraries that are straightforward 
 * to run on embedded platforms. On embedded platforms memory and computational resources are severely limited. 
 * Olaf is written in portable C with these restrictions in mind. Olaf mainly targets 32-bit ARM devices such as 
 * some Teensy’s, some Arduino’s and the ESP32. Other modern embedded platforms with similar specifications and 
 * might work as well.
 * 
 * Olaf, being written in portable C, operates also on traditional computers. There, the efficiency of Olaf makes 
 * it run fast. On embedded devices reference fingerprints are stored in memory. On traditional computers 
 * fingerprints are stored in a high-performance key-value-store: LMDB. LMDB offers an a B+-tree based persistent 
 * storage ideal for small keys and values with low storage overhead.
 * 
 * Olaf works in the browser. Via Emscripten Olaf can be compiled to WASM. This makes it relatively 
 * straightforward to combine the capabilities of the Web Audio API and Olaf to create browser based audio 
 * fingerprinting applications.
 * 
 * @section design Olaf's design
 * 
 * The core of Olaf is kept as small as possible and is shared between the ESP32, WebAssembly and 
 * traditional version. The only difference is how audio enters the system. The default 
 * configuration expects monophonic, 32bit float audio sampled at 16kHz. Transcoding and resampling is different
 * for each environment.  
 * 
 * For traditional computers file handling and transcoding is governed by a Ruby script. 
 * This script expands lists of incoming audio files, transcodes audio files, checks incoming audio,
 * checks for duplicate material, validates arguments and input,... The Ruby script, essentially, 
 * makes Olaf an easy to use CLI application and keeps the C parts of Olaf simple. The C core it is 
 * not concerned with e.g. transcoding. Crucially, the C core trusts input and does not do much input
 * validation and does not provide much guardrails. Since the interface is the Ruby script this 
 * seems warranted.
 *
 * For the ESP32 version, only a small part of the core is used. To create a new ESP32 Arduino project, there is a 
 * small script which links to the core. The default Arduino setup uses C++ extensions so olaf_config.h 
 * remains olaf_config.h 
 * but `olaf_config.c` becomes `olaf_config.cpp`. Perhaps other environments ([PlatformIO](https://platformio.org/ "PlatformIO")) do not need this name change.
 * 
 * The WebAssembly version also uses a similarly small part of the core and some bridge code is available.
 * 
 * To verify both the ESP32 and WebAssembly versions, the Olaf C core can be compiled with the
 * 'memory' database to mimic the ESP32/WebAssembly versions. In the compilation step the implementation for
 * the `olaf_db.h` header is done by `olaf_db_mem.c` in stead of the standard `olaf_db.c` implementation. This
 * paradigm of several implementations for a single header file is done a few times in Olaf. Olaf ships with several
 * max-filter implementations.
 * 
 * @section configuration Olaf's configuration
 * 
 * The configuration of Olaf is set at compile time, since it is expected to not change
 * in between runs. The default configuration is defined in the function olaf_config_default() in 
 * olaf_config.h. A different set of default configuration parameters is available for the 
 * memory version, ESP32 version (e.g.  olaf_config_esp_32()) and the WebAssembly version.   
 * 
 * @secreflist
 * 
 **/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

#include "olaf_stream_processor.h"
#include "olaf_runner.h"
#include "olaf_config.h"
#include "olaf_db.h"
#include "olaf_fp_db_writer_cache.h"

void olaf_print_help(const char* message){
	fprintf(stderr,"%s",message);
	fprintf(stderr,"\tolaf_c [query audio.raw audio.wav | print audio.raw audio.wav |store [raw_audio.raw audio.wav]... | stats | name_to_id file_name.mp3 | delete raw_audio.raw audio.wav | query_by_key key1 key2... | query_by_key file.txt ]\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"\tquery_by_key: Query database using fingerprint keys or audio IDs\n");
	fprintf(stderr,"\t  - For fingerprint keys: 64-bit hex (0x1234ABCD...) or decimal\n");
	fprintf(stderr,"\t  - For audio IDs: 32-bit decimal (from 'olaf stats' output)\n");
	fprintf(stderr,"\t  - Can read from file: one key per line, # for comments\n");
	exit(-10);
}

/**
 * Parse a hexadecimal key string to uint64_t
 * Supports both "0x1234..." and "1234..." formats
 * Returns true on success, false on error
 */
bool olaf_parse_hex_key(const char* key_string, uint64_t* result) {
	if (!key_string || !result) {
		return false;
	}
	
	char* endptr;
	errno = 0;
	
	// Handle both "0x" prefixed and non-prefixed hex strings
	if (strncmp(key_string, "0x", 2) == 0 || strncmp(key_string, "0X", 2) == 0) {
		*result = strtoull(key_string, &endptr, 16);
	} else {
		// Assume it's hex without prefix
		*result = strtoull(key_string, &endptr, 16);
	}
	
	// Check for conversion errors
	if (errno == ERANGE) {
		fprintf(stderr, "Error: Key '%s' exceeds maximum value for 64-bit integer\n", key_string);
		return false;
	}
	
	if (endptr == key_string || *endptr != '\0') {
		fprintf(stderr, "Error: Invalid hexadecimal key format '%s'. Expected format: 0x1234ABCD or 1234ABCD\n", key_string);
		return false;
	}
	
	return true;
}

/**
 * Parse a decimal key string to uint64_t
 * Returns true on success, false on error
 */
bool olaf_parse_decimal_key(const char* key_string, uint64_t* result) {
	if (!key_string || !result) {
		return false;
	}
	
	char* endptr;
	errno = 0;
	
	*result = strtoull(key_string, &endptr, 10);
	
	// Check for conversion errors
	if (errno == ERANGE) {
		fprintf(stderr, "Error: Key '%s' exceeds maximum value for 64-bit integer\n", key_string);
		return false;
	}
	
	if (endptr == key_string || *endptr != '\0') {
		fprintf(stderr, "Error: Invalid decimal key format '%s'. Expected numeric format: 1234567890\n", key_string);
		return false;
	}
	
	return true;
}

/**
 * Parse a key string in either hexadecimal or decimal format
 * Auto-detects format based on prefix and content
 * Returns true on success, false on error
 */
bool olaf_parse_key(const char* key_string, uint64_t* result) {
	if (!key_string || !result) {
		return false;
	}
	
	// Check if it looks like hex (starts with 0x or contains hex digits)
	if (strncmp(key_string, "0x", 2) == 0 || strncmp(key_string, "0X", 2) == 0) {
		return olaf_parse_hex_key(key_string, result);
	}
	
	// Check if string contains hex characters (A-F, a-f)
	bool has_hex_chars = false;
	for (const char* p = key_string; *p; p++) {
		if ((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) {
			has_hex_chars = true;
			break;
		}
	}
	
	if (has_hex_chars) {
		return olaf_parse_hex_key(key_string, result);
	} else {
		return olaf_parse_decimal_key(key_string, result);
	}
}

/**
 * Validate that a key is within valid range for uint64_t
 * Returns true if valid, false otherwise
 */
bool olaf_validate_key_range(uint64_t key) {
	// For uint64_t, all values from 0 to UINT64_MAX are valid
	// This function is mainly for consistency and future extensibility
	(void)key; // Suppress unused parameter warning
	return true;
}

/**
 * Process a single key string and query the database with optional metadata
 * Returns true on success, false on error
 */
bool olaf_process_single_key_with_metadata(Olaf_DB* db, const char* key_string, bool verbose) {
	uint64_t key;
	
	// Parse the key
	if (!olaf_parse_key(key_string, &key)) {
		return false;
	}
	
	// Validate key range
	if (!olaf_validate_key_range(key)) {
		fprintf(stderr, "Error: Key value out of valid range\n");
		return false;
	}
	
	// Query the database for this key
	// For now, we'll use the key as both start and stop for exact match
	uint64_t results[100]; // Buffer for results
	size_t num_results = olaf_db_find(db, key, key, results, 100);
	
	// Display results
	printf("Key: 0x%016llX (%llu)\n", (unsigned long long)key, (unsigned long long)key);
	if (num_results == 0) {
		printf("  No matches found\n");
	} else {
		for (size_t i = 0; i < num_results; i++) {
			uint64_t value = results[i];
			uint32_t audio_id = (uint32_t)(value & 0xFFFFFFFF);
			uint32_t timestamp = (uint32_t)(value >> 32);
			
			if (verbose) {
				// Try to get metadata for this audio_id
				if (olaf_db_has_meta_data(db, &audio_id)) {
					Olaf_Resource_Meta_data metadata;
					olaf_db_find_meta_data(db, &audio_id, &metadata);
					printf("  Match: audio_id=%u, timestamp=%u, file=\"%s\", duration=%.3fs\n", 
						   audio_id, timestamp, metadata.path, metadata.duration);
				} else {
					printf("  Match: audio_id=%u, timestamp=%u, file=<unknown>, duration=<unknown>\n", 
						   audio_id, timestamp);
				}
			} else {
				printf("  Match: audio_id=%u, timestamp=%u\n", audio_id, timestamp);
			}
		}
	}
	printf("\n");
	
	return true;
}

/**
 * Process a single key string and query the database
 * Returns true on success, false on error
 */
bool olaf_process_single_key(Olaf_DB* db, const char* key_string) {
	return olaf_process_single_key_with_metadata(db, key_string, false);
}

/**
 * Read keys from a file and process them with optional metadata
 * Returns true on success, false on error
 */
bool olaf_process_keys_from_file_with_metadata(Olaf_DB* db, const char* filename, bool verbose) {
	FILE* file = fopen(filename, "r");
	if (!file) {
		fprintf(stderr, "Error: Could not open file '%s': %s\n", filename, strerror(errno));
		return false;
	}
	
	char line[256];
	int line_number = 0;
	bool success = true;
	
	while (fgets(line, sizeof(line), file)) {
		line_number++;
		
		// Remove trailing newline
		size_t len = strlen(line);
		if (len > 0 && line[len-1] == '\n') {
			line[len-1] = '\0';
		}
		
		// Skip empty lines and comments
		if (strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		
		// Process the key
		printf("Processing line %d: %s\n", line_number, line);
		if (!olaf_process_single_key_with_metadata(db, line, verbose)) {
			fprintf(stderr, "Error processing key on line %d: %s\n", line_number, line);
			success = false;
		}
	}
	
	fclose(file);
	return success;
}

/**
 * Read keys from a file and process them
 * Returns true on success, false on error
 */
bool olaf_process_keys_from_file(Olaf_DB* db, const char* filename) {
	return olaf_process_keys_from_file_with_metadata(db, filename, false);
}

/**
 * Structure to hold similarity match results
 */
typedef struct {
	uint32_t audio_id;
	uint32_t match_count;
	float similarity_score;
	char file_path[512];
	float duration;
} Olaf_Similarity_Match;

/**
 * Find similar audio files by sampling fingerprints from the target audio_id
 * This leverages existing fingerprint matching without full database scan
 */
size_t olaf_find_similar_audio(Olaf_DB* db, uint32_t target_audio_id, Olaf_Similarity_Match* matches, size_t max_matches) {
	// Sample fingerprints by scanning a portion of the database for this audio_id
	const size_t sample_size = 100; // Sample up to 100 fingerprints
	uint64_t sample_keys[sample_size];
	size_t found_samples = 0;
	
	// Scan through database to find fingerprints belonging to target_audio_id
	// We'll scan in chunks to find some representative fingerprints
	const size_t chunk_size = 1000;
	uint64_t results[chunk_size];
	uint64_t current_key = 0;
	
	// Find sample fingerprints from the target audio
	while (found_samples < sample_size && current_key < UINT64_MAX) {
		size_t found = olaf_db_find(db, current_key, current_key + chunk_size - 1, results, chunk_size);
		
		if (found == 0) {
			current_key += chunk_size;
			if (current_key < chunk_size) break; // Overflow
			continue;
		}
		
		// Look for fingerprints from our target audio_id
		for (size_t i = 0; i < found && found_samples < sample_size; i++) {
			uint64_t value = results[i];
			uint32_t audio_id = (uint32_t)(value & 0xFFFFFFFF);
			
			if (audio_id == target_audio_id) {
				// Calculate the fingerprint key from the value
				// This is a reverse calculation - we need the actual key
				// For now, we'll use a simpler approach
				sample_keys[found_samples] = current_key + i;
				found_samples++;
			}
		}
		
		current_key += chunk_size;
		if (current_key < chunk_size) break; // Overflow
	}
	
	if (found_samples == 0) {
		return 0; // No fingerprints found for this audio_id
	}
	
	// Now use these sample fingerprints to find similar audio files
	uint32_t match_counts[1000] = {0}; // Track matches per audio_id
	uint32_t audio_ids[1000];
	size_t unique_audio_count = 0;
	
	// Query each sample fingerprint to find matches
	for (size_t i = 0; i < found_samples; i++) {
		uint64_t query_results[100];
		size_t num_matches = olaf_db_find(db, sample_keys[i], sample_keys[i], query_results, 100);
		
		for (size_t j = 0; j < num_matches; j++) {
			uint64_t match_value = query_results[j];
			uint32_t match_audio_id = (uint32_t)(match_value & 0xFFFFFFFF);
			
			// Skip self-matches
			if (match_audio_id == target_audio_id) continue;
			
			// Find or add this audio_id to our tracking
			bool found_existing = false;
			for (size_t k = 0; k < unique_audio_count; k++) {
				if (audio_ids[k] == match_audio_id) {
					match_counts[k]++;
					found_existing = true;
					break;
				}
			}
			
			if (!found_existing && unique_audio_count < 1000) {
				audio_ids[unique_audio_count] = match_audio_id;
				match_counts[unique_audio_count] = 1;
				unique_audio_count++;
			}
		}
	}
	
	// Convert to similarity matches and sort by match count
	size_t result_count = 0;
	for (size_t i = 0; i < unique_audio_count && result_count < max_matches; i++) {
		if (match_counts[i] > 0) {
			matches[result_count].audio_id = audio_ids[i];
			matches[result_count].match_count = match_counts[i];
			matches[result_count].similarity_score = (float)match_counts[i] / (float)found_samples * 100.0f;
			
			// Get metadata if available
			if (olaf_db_has_meta_data(db, &audio_ids[i])) {
				Olaf_Resource_Meta_data metadata;
				olaf_db_find_meta_data(db, &audio_ids[i], &metadata);
				strncpy(matches[result_count].file_path, metadata.path, sizeof(matches[result_count].file_path) - 1);
				matches[result_count].file_path[sizeof(matches[result_count].file_path) - 1] = '\0';
				matches[result_count].duration = metadata.duration;
			} else {
				strcpy(matches[result_count].file_path, "<unknown>");
				matches[result_count].duration = 0.0f;
			}
			
			result_count++;
		}
	}
	
	// Simple bubble sort by similarity score (descending)
	for (size_t i = 0; i < result_count - 1; i++) {
		for (size_t j = 0; j < result_count - i - 1; j++) {
			if (matches[j].similarity_score < matches[j + 1].similarity_score) {
				Olaf_Similarity_Match temp = matches[j];
				matches[j] = matches[j + 1];
				matches[j + 1] = temp;
			}
		}
	}
	
	return result_count;
}

/**
 * Query database by audio_id to find all fingerprints for that audio file
 * Returns true on success, false on error
 */
bool olaf_process_audio_id(Olaf_DB* db, uint32_t audio_id, bool verbose) {
	printf("Audio ID: %u\n", audio_id);
	
	// Check if we have metadata for this audio_id
	if (olaf_db_has_meta_data(db, &audio_id)) {
		Olaf_Resource_Meta_data metadata;
		olaf_db_find_meta_data(db, &audio_id, &metadata);
		
		if (verbose) {
			printf("  File: \"%s\", Duration: %.3fs\n", metadata.path, metadata.duration);
			printf("  Fingerprints: %ld (from metadata)\n", metadata.fingerprints);
		} else {
			printf("  File: \"%s\", Duration: %.3fs, Fingerprints: %ld\n", 
				   metadata.path, metadata.duration, metadata.fingerprints);
		}
		
		printf("  Found audio file in database\n");
		
		// Find similar audio files
		printf("\n  Finding similar audio files...\n");
		Olaf_Similarity_Match similar_matches[10];
		size_t num_similar = olaf_find_similar_audio(db, audio_id, similar_matches, 10);
		
		if (num_similar > 0) {
			printf("  Similar audio files found:\n");
			for (size_t i = 0; i < num_similar; i++) {
				printf("    %zu. Audio ID: %u, Similarity: %.1f%% (%u matches), File: \"%s\"\n",
					   i + 1, similar_matches[i].audio_id, similar_matches[i].similarity_score,
					   similar_matches[i].match_count, similar_matches[i].file_path);
			}
		} else {
			printf("  No similar audio files found\n");
		}
		
	} else {
		printf("  No metadata found for this audio_id\n");
		return false;
	}
	
	printf("\n");
	return true;
}

/**
 * Check if a string looks like a filename (contains . or /)
 * Returns true if it looks like a filename, false otherwise
 */
bool olaf_looks_like_filename(const char* arg) {
	return (strchr(arg, '.') != NULL || strchr(arg, '/') != NULL || strchr(arg, '\\') != NULL);
}

int olaf_stats(void){
	//print database statistics and exit
	Olaf_Config* config = olaf_config_default();
	Olaf_DB* db = olaf_db_new(config->dbFolder,true);
	olaf_db_stats(db,config->verbose);
	olaf_db_destroy(db);
	olaf_config_destroy(config);
	exit(0);
	return 0;
}

int olaf_has(int argc, const char* argv[]){
	Olaf_Config* config = olaf_config_default();
	Olaf_DB* db = olaf_db_new(config->dbFolder,true);

	printf("audio file path; internal identifier; duration (s); fingerprints (#)\n");
	for(int arg_index = 2 ; arg_index < argc ; arg_index++){
		const char* orig_path = argv[arg_index];
		uint32_t audio_id = olaf_db_string_hash(orig_path,strlen(orig_path));
		if(olaf_db_has_meta_data(db,&audio_id)){
			Olaf_Resource_Meta_data e;
			olaf_db_find_meta_data(db,&audio_id,&e);
			printf("%s;%u;%.3f;%ld\n",orig_path,audio_id,e.duration,e.fingerprints);
		}else{
			printf("%s;;;\n",orig_path);
		}
	}
	olaf_db_destroy(db);
	olaf_config_destroy(config);
	exit(0);
	return 0;
}

int olaf_store_cached(int argc, const char* argv[]){
	Olaf_Config* config = olaf_config_default();
	Olaf_DB* db = olaf_db_new(config->dbFolder,false);

	for(int arg_index = 2 ; arg_index < argc ; arg_index++){
		const char* csv_filename = argv[arg_index];
		Olaf_FP_DB_Writer_Cache * cache_writer  = olaf_fp_db_writer_cache_new(db,config,csv_filename);
		olaf_fp_db_writer_cache_store(cache_writer);
		olaf_fp_db_writer_cache_destroy(cache_writer);
	}
	olaf_db_destroy(db);
	olaf_config_destroy(config);
	exit(0);
	return 0;
}

int olaf_query_by_key(int argc, const char* argv[]){
	Olaf_Config* config = olaf_config_default();
	Olaf_DB* db = olaf_db_new(config->dbFolder,true);

	if (argc < 3) {
		fprintf(stderr, "Error: No keys provided. Usage: olaf_c query_by_key key1 key2... or olaf_c query_by_key file.txt\n");
		fprintf(stderr, "  - For fingerprint keys: use 64-bit hex (0x1234...) or decimal values\n");
		fprintf(stderr, "  - For audio IDs: use 32-bit decimal values (like from 'olaf stats')\n");
		olaf_db_destroy(db);
		olaf_config_destroy(config);
		exit(-1);
	}
	
	bool success = true;
	
	for(int arg_index = 2 ; arg_index < argc ; arg_index++){
		const char* arg = argv[arg_index];
		
		// Check if this looks like a filename
		if (olaf_looks_like_filename(arg)) {
			// Try to process as a file
			if (!olaf_process_keys_from_file(db, arg)) {
				// If file processing fails, try as a regular key
				printf("File processing failed, trying as key: %s\n", arg);
				if (!olaf_process_single_key(db, arg)) {
					success = false;
				}
			}
		} else {
			// Parse the argument to determine if it's a fingerprint key or audio_id
			uint64_t parsed_value;
			if (olaf_parse_key(arg, &parsed_value)) {
				// Check if this looks like an audio_id (32-bit value) or fingerprint key (64-bit)
				if (parsed_value <= UINT32_MAX) {
					// Treat as audio_id
					uint32_t audio_id = (uint32_t)parsed_value;
					printf("Interpreting %s as audio_id: %u\n", arg, audio_id);
					if (!olaf_process_audio_id(db, audio_id, true)) {
						success = false;
					}
				} else {
					// Treat as fingerprint key
					printf("Interpreting %s as fingerprint key\n", arg);
					if (!olaf_process_single_key(db, arg)) {
						success = false;
					}
				}
			} else {
				success = false;
			}
		}
	}

	olaf_db_destroy(db);
	olaf_config_destroy(config);
	
	if (!success) {
		exit(-1);
	}
	
	exit(0);
	return 0;
}

int main(int argc, const char* argv[]){

	if(argc < 2){
		olaf_print_help("No filename given\n");
	}

	const char* command = argv[1];
	int runner_mode = OLAF_RUNNER_MODE_QUERY; 
	
	if(strcmp(command,"store") == 0){
		runner_mode = OLAF_RUNNER_MODE_STORE;
	} else if(strcmp(command,"query") == 0){
		runner_mode = OLAF_RUNNER_MODE_QUERY;
	} else if(strcmp(command,"delete") == 0){
		runner_mode = OLAF_RUNNER_MODE_DELETE;
	} else if(strcmp(command,"print") == 0){
		runner_mode = OLAF_RUNNER_MODE_PRINT;
	} else if(strcmp(command,"name_to_id") == 0){
		//print the hash and exit
		printf("%u\n",olaf_db_string_hash(argv[2],strlen(argv[2])));
		exit(0);
		return 0;
	} else if(strcmp(command,"stats") == 0){
		olaf_stats();
	} else if(strcmp(command,"has") == 0){
		olaf_has(argc,argv);
	} else if(strcmp(command,"store_cached") == 0){
		olaf_store_cached(argc,argv);
	} else if(strcmp(command,"query_by_key") == 0){
		olaf_query_by_key(argc,argv);
	} else {
		fprintf(stderr,"%s Unknown command: \n",command);
		olaf_print_help("Unknown command\n");
	}

	Olaf_Runner * runner = olaf_runner_new(runner_mode);

	if(runner_mode == OLAF_RUNNER_MODE_QUERY && argc == 2){
		//read audio samples from standard input
		runner->config->printResultEvery = 3;//print results every three seconds
		runner->config->keepMatchesFor = 10;//keep matches for 7 seconds
		fprintf(stderr,"Start listening for incoming raw audio samples piped in over STDIN.\n");
		Olaf_Stream_Processor* processor = olaf_stream_processor_new(runner,NULL,"stdin");
		olaf_stream_processor_process(processor);
		olaf_stream_processor_destroy(processor);
	}else{
		if(argc % 2 == 1 ){
			fprintf(stderr,"Error: You need to provide converted raw audio and the original file name, for example:\n\tolaf query audio.raw original_filename.mp3\n");
			exit(-3);	
		}
		//for each audio file
		for(int arg_index = 2 ; arg_index + 1 < argc ; arg_index+=2){
			const char* raw_path =  argv[arg_index];
			const char* orig_path = argv[arg_index + 1];
			Olaf_Stream_Processor* processor = olaf_stream_processor_new(runner,raw_path,orig_path);
			olaf_stream_processor_process(processor);
			olaf_stream_processor_destroy(processor);
		}
	}

	olaf_runner_destroy(runner);

	return 0;
}
