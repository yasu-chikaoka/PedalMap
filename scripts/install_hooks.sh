#!/bin/bash

# Define hook path and content
HOOK_PATH=".git/hooks/pre-commit"
EXPECTED_CONTENT='#!/bin/bash

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format is not installed or not in PATH."
    exit 1
fi

# Get list of staged files that match the extensions
files=$(git diff --cached --name-only --diff-filter=ACM | grep -E "\.(c|cpp|cc|h|hpp|java|js|ts|proto)$")

if [ -z "$files" ]; then
    exit 0
fi

# Format files
for file in $files; do
    if [ -f "$file" ]; then
        clang-format -i "$file"
        git add "$file"
    fi
done'

# Install hook
echo "$EXPECTED_CONTENT" > "$HOOK_PATH"
chmod +x "$HOOK_PATH"

echo "✅ Pre-commit hook installed successfully at $HOOK_PATH"
echo "Please verify that clang-format is installed:"
if command -v clang-format &> /dev/null; then
    echo "  ✅ Found clang-format: $(clang-format --version)"
else
    echo "  ❌ clang-format not found. Please install it:"
    echo "  - macOS: brew install clang-format"
    echo "  - Linux: sudo apt-get install clang-format"
fi
