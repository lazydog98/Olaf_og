# Query with individual keys
olaf query_by_key 0x1234ABCD 9876543210

# Query from file
olaf query_by_key keys.txt

# Mixed usage
olaf query_by_key 0x1234 keys.txt 9876543210

# With verbose metadata (Ruby CLI)
olaf query_by_key --verbose 0x1234ABCD


# keys.txt example
0x1234567890ABCDEF
1311768467294899695
# This is a comment
0xABCDEF1234567890
