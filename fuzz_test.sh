#!/bin/bash
# Fuzz test for printable_binary
# Tests encoding/decoding of large random binary data

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

SCRIPT="./printable_binary"
TEST_SIZE_MB=5
TEST_SIZE_BYTES=$((TEST_SIZE_MB * 1024 * 1024))

echo -e "${BLUE}=== PrintableBinary Fuzz Test ===${NC}"
echo -e "Testing with ${TEST_SIZE_MB}MB of random data..."

# Create temporary files
RANDOM_DATA=$(mktemp)
ENCODED_DATA=$(mktemp)
DECODED_DATA=$(mktemp)

# Cleanup function
cleanup() {
    echo -e "Cleaning up temporary files..."
    rm -f "$RANDOM_DATA" "$ENCODED_DATA" "$DECODED_DATA"
}
trap cleanup EXIT

# Generate random data - using /dev/urandom for speed
# Use /dev/random instead if you want higher quality randomness
echo -e "${YELLOW}Generating ${TEST_SIZE_MB}MB of random data...${NC}"
dd if=/dev/urandom of="$RANDOM_DATA" bs=1M count="$TEST_SIZE_MB" 2>/dev/null

# Calculate hash of the original data
echo -e "${YELLOW}Calculating hash of original data...${NC}"
ORIGINAL_HASH=$(shasum -a 256 "$RANDOM_DATA" | cut -d ' ' -f 1)
echo "Original hash: $ORIGINAL_HASH"

# Encode the data
echo -e "${YELLOW}Encoding data...${NC}"
time $SCRIPT "$RANDOM_DATA" > "$ENCODED_DATA"
ENCODED_SIZE=$(wc -c < "$ENCODED_DATA")
ENCODED_SIZE_MB=$(echo "scale=2; $ENCODED_SIZE / (1024*1024)" | bc)
echo "Encoded size: ${ENCODED_SIZE_MB}MB (${ENCODED_SIZE} bytes)"

# Calculate compression ratio
RATIO=$(echo "scale=2; $ENCODED_SIZE / $TEST_SIZE_BYTES" | bc)
echo "Encoding ratio: ${RATIO}x"

# Decode the data
echo -e "${YELLOW}Decoding data...${NC}"
time $SCRIPT -d "$ENCODED_DATA" > "$DECODED_DATA"

# Calculate hash of the decoded data
echo -e "${YELLOW}Calculating hash of decoded data...${NC}"
DECODED_HASH=$(shasum -a 256 "$DECODED_DATA" | cut -d ' ' -f 1)
echo "Decoded hash: $DECODED_HASH"

# Compare hashes
if [ "$ORIGINAL_HASH" = "$DECODED_HASH" ]; then
    echo -e "${GREEN}TEST PASSED: Hashes match!${NC}"
    echo "The encoding/decoding process perfectly preserved the data."
else
    echo -e "${RED}TEST FAILED: Hashes don't match!${NC}"
    echo "Original: $ORIGINAL_HASH"
    echo "Decoded: $DECODED_HASH"
    exit 1
fi

echo -e "\n${BLUE}All tests completed successfully.${NC}"