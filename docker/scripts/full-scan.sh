#!/bin/bash

# =============================================================================
# CodeQL C++ Analysis Script
# Automated security analysis for C++ projects like LumexLib
# =============================================================================

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default configuration
PROJECT_NAME="${PROJECT_NAME:-LumexLib}"
SOURCE_DIR="${1:-/workspace/lumexlib}"
DATABASE_DIR="${DATABASE_DIR:-/tmp/codeql-db}"
OUTPUT_DIR="${OUTPUT_DIR:-/tmp/results}"
THREADS="${CODEQL_THREADS:-$(nproc)}"
RAM="${CODEQL_RAM:-4096}"
CPP_STANDARD="${CPP_STANDARD:-11}"
BUILD_MODE="${BUILD_MODE:-autobuild}"
EXCLUDE_TESTS="${EXCLUDE_TESTS:-true}"

# Analysis configuration
CUSTOM_QUERIES="/opt/codeql-config"
CONFIG_FILE="/opt/codeql-config/codeql-config.yml"

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_header() {
    echo -e "\n${PURPLE}========================================${NC}"
    echo -e "${PURPLE} $1${NC}"
    echo -e "${PURPLE}========================================${NC}\n"
}

# Help function
show_help() {
    cat <<EOF
${CYAN}CodeQL C++ Analysis Script${NC}

${YELLOW}USAGE:${NC}
    full-scan.sh [SOURCE_DIR] [OPTIONS]

${YELLOW}ARGUMENTS:${NC}
    SOURCE_DIR          Source code directory (default: /workspace/lumexlib)

${YELLOW}OPTIONS:${NC}
    -h, --help          Show this help message
    -o, --output DIR    Output directory for results (default: /tmp/results)
    -d, --database DIR  Database directory (default: /tmp/codeql-db)
    -t, --threads N     Number of threads (default: $(nproc))
    -m, --memory MB     Memory limit in MB (default: 4096)
    -s, --standard VER  C++ standard version (default: 11)
    -q, --queries PATH  Custom queries directory
    -c, --config FILE   CodeQL config file
    --build-mode MODE   Build mode: none|autobuild|manual (default: none)
    --include-tests     Include test files in analysis
    --clean             Clean previous database before analysis

${YELLOW}EXAMPLES:${NC}
    # Basic analysis
    full-scan.sh

    # Custom source directory
    full-scan.sh /workspace/myproject

    # Full analysis with custom config
    full-scan.sh -c /opt/custom-config.yml --threads 8

${YELLOW}ENVIRONMENT VARIABLES:${NC}
    PROJECT_NAME        Project name for reporting
    CODEQL_THREADS      Default thread count
    CODEQL_RAM          Default memory limit
    CPP_STANDARD        C++ standard version
    BUILD_MODE          Build mode
    EXCLUDE_TESTS       Exclude test files (true/false)

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
        -h | --help)
            show_help
            exit 0
            ;;
        -o | --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -d | --database)
            DATABASE_DIR="$2"
            shift 2
            ;;
        -t | --threads)
            THREADS="$2"
            shift 2
            ;;
        -m | --memory)
            RAM="$2"
            shift 2
            ;;
        -s | --standard)
            CPP_STANDARD="$2"
            shift 2
            ;;
        -q | --queries)
            CUSTOM_QUERIES="$2"
            shift 2
            ;;
        -c | --config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        --build-mode)
            BUILD_MODE="$2"
            shift 2
            ;;
        --include-tests)
            EXCLUDE_TESTS="false"
            shift
            ;;
        --clean)
            CLEAN_DB="true"
            shift
            ;;
        -*)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
        *)
            if [[ -z "${SOURCE_DIR_SET:-}" ]]; then
                SOURCE_DIR="$1"
                SOURCE_DIR_SET=true
            else
                log_error "Unexpected argument: $1"
                exit 1
            fi
            shift
            ;;
        esac
    done
}

# Validate inputs
validate_inputs() {
    log_info "Validating inputs..."

    if [[ ! -d "$SOURCE_DIR" ]]; then
        log_error "Source directory does not exist: $SOURCE_DIR"
        exit 1
    fi

    if [[ ! -f "$CONFIG_FILE" ]] && [[ "$CONFIG_FILE" != "/opt/codeql-config/codeql-config.yml" ]]; then
        log_warning "Config file not found: $CONFIG_FILE, using default"
        CONFIG_FILE="/opt/codeql-config/codeql-config.yml"
    fi

    # Create output directories
    mkdir -p "$OUTPUT_DIR" "$DATABASE_DIR"

    log_success "Input validation complete"
}

# Clean previous database
clean_database() {
    if [[ "${CLEAN_DB:-false}" == "true" ]]; then
        log_info "Cleaning previous database..."
        rm -rf "${DATABASE_DIR:?}"/*
        log_success "Database cleaned"
    fi
}

# Create CodeQL database
create_database() {
    log_header "Creating CodeQL Database"

    local db_path="${DATABASE_DIR}/${PROJECT_NAME}-cpp"

    # Create a build directory
    local build_dir="${SOURCE_DIR}/codeql-build"
    mkdir -p "$build_dir"

    # Configure CMake
    log_info "Configuring CMake build..."
    (cd "$build_dir" && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DLUMEX_BUILD_TESTS=OFF \
        -DLUMEX_BUILD_SHARED_LIBS=ON \
        -DCMAKE_CXX_STANDARD=$CPP_STANDARD \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON) || log_warning "CMake configuration had issues"

    # Create database with full build
    local build_cmd="make -j${THREADS} || cmake --build . --parallel ${THREADS}"

    local create_cmd=(
        codeql database create
        "$db_path"
        --language=cpp
        --source-root="$SOURCE_DIR"
        --working-dir="$build_dir"
        --command="/bin/bash -c \"${build_cmd}\""
        --threads="$THREADS"
        --ram="$RAM"
        --overwrite
    )

    log_info "Creating database with full project build..."
    log_info "This will compile all source files in your library"

    if "${create_cmd[@]}"; then
        log_success "Database created successfully: $db_path"
        echo "$db_path" >"${OUTPUT_DIR}/database-path.txt"

        # Clean up build directory
        rm -rf "$build_dir"
    else
        log_error "Failed to create database"
        # Try fallback method
        log_info "Trying fallback database creation without build..."

        codeql database create "$db_path" \
            --language=cpp \
            --source-root="$SOURCE_DIR" \
            --command="" \
            --threads="$THREADS" \
            --ram="$RAM" \
            --overwrite

        if [ $? -eq 0 ]; then
            log_warning "Database created without build - analysis may be limited"
        else
            exit 1
        fi
    fi
}

# Run CodeQL analysis
run_analysis() {
    log_header "Running CodeQL Analysis"

    local db_path="${DATABASE_DIR}/${PROJECT_NAME}-cpp"
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local sarif_output="${OUTPUT_DIR}/${PROJECT_NAME}_${timestamp}.sarif"
    local csv_output="${OUTPUT_DIR}/${PROJECT_NAME}_${timestamp}.csv"

    # Check if database exists
    if [[ ! -d "$db_path" ]]; then
        log_error "Database not found: $db_path"
        exit 1
    fi

    # Analyze with security queries
    local analyze_cmd=(
        codeql database analyze
        "$db_path"
        --format=sarif-latest
        --output="$sarif_output"
        --threads="$THREADS"
        --ram="$RAM"
    )

    # Add query suites based on configuration
    if [[ -f "$CONFIG_FILE" ]]; then
        analyze_cmd+=("--sarif-category=config" "--config-file=$CONFIG_FILE")
    else
        # Use standard security queries
        analyze_cmd+=("cpp-security-and-quality" "cpp-security-extended")
    fi

    # Add custom queries if directory exists
    if [[ -d "$CUSTOM_QUERIES" ]] && [[ -f "$CUSTOM_QUERIES/custom-queries.ql" ]]; then
        analyze_cmd+=("$CUSTOM_QUERIES/custom-queries.ql")
        log_info "Including custom queries from: $CUSTOM_QUERIES"
    fi

    log_info "Running security analysis on all library files..."
    log_info "This may take several minutes depending on codebase size..."

    if "${analyze_cmd[@]}"; then
        log_success "Analysis completed successfully"

        # Also generate CSV output
        log_info "Generating CSV report..."
        codeql database analyze "$db_path" \
            --format=csv \
            --output="$csv_output" \
            --threads="$THREADS" \
            --ram="$RAM" \
            cpp-security-and-quality || log_warning "CSV generation had issues"

        # Create symlinks to latest results
        ln -sf "$(basename "$sarif_output")" "${OUTPUT_DIR}/latest.sarif"
        ln -sf "$(basename "$csv_output")" "${OUTPUT_DIR}/latest.csv"

        log_info "Results saved:"
        log_info "- SARIF: $sarif_output"
        log_info "- CSV: $csv_output"

    else
        log_error "Analysis failed"
        exit 1
    fi
}

# Process and summarize results
process_results() {
    log_header "Processing Results"

    local sarif_file="${OUTPUT_DIR}/latest.sarif"

    if [[ ! -f "$sarif_file" ]]; then
        log_error "SARIF results file not found: $sarif_file"
        return 1
    fi

    # Extract summary using jq
    local total_results
    local critical_count=0
    local high_count=0
    local medium_count=0
    local low_count=0

    if command -v jq &>/dev/null; then
        total_results=$(jq '.runs[].results | length' "$sarif_file" 2>/dev/null || echo "0")

        # Count by security severity (approximate)
        critical_count=$(jq -r '.runs[].results[] | select(.level == "error" and (.properties.securitySeverity // 0) >= 9.0) | .ruleId' "$sarif_file" 2>/dev/null | wc -l || echo "0")
        high_count=$(jq -r '.runs[].results[] | select(.level == "error" and (.properties.securitySeverity // 0) >= 7.0 and (.properties.securitySeverity // 0) < 9.0) | .ruleId' "$sarif_file" 2>/dev/null | wc -l || echo "0")
        medium_count=$(jq -r '.runs[].results[] | select(.level == "warning" and (.properties.securitySeverity // 0) >= 4.0) | .ruleId' "$sarif_file" 2>/dev/null | wc -l || echo "0")
        low_count=$(jq -r '.runs[].results[] | select(.level == "warning" and (.properties.securitySeverity // 0) < 4.0) | .ruleId' "$sarif_file" 2>/dev/null | wc -l || echo "0")

        # Generate summary
        cat >"${OUTPUT_DIR}/summary.txt" <<EOF
CodeQL Security Analysis Summary
================================
Project: $PROJECT_NAME
Date: $(date)
Source: $SOURCE_DIR
Database: ${DATABASE_DIR}/${PROJECT_NAME}-cpp

Results Overview:
- Total findings: $total_results
- 🔴 Critical: $critical_count
- 🟠 High: $high_count  
- 🟡 Medium: $medium_count
- 🟢 Low: $low_count

Files:
- SARIF: $sarif_file
- CSV: ${OUTPUT_DIR}/latest.csv
- Summary: ${OUTPUT_DIR}/summary.txt
EOF

        log_success "Analysis Summary:"
        cat "${OUTPUT_DIR}/summary.txt"

        # Return exit code based on critical/high findings
        if [[ $critical_count -gt 0 || $high_count -gt 0 ]]; then
            log_warning "Critical or high severity vulnerabilities found!"
            return 2
        fi

    else
        log_warning "jq not available, skipping detailed summary"
        echo "Total results in SARIF: $(grep -c '"level"' "$sarif_file" || echo "unknown")"
    fi

    log_success "Results processing complete"
}

# Main execution
main() {
    log_header "CodeQL C++ Security Analysis"
    log_info "Project: $PROJECT_NAME"
    log_info "Source: $SOURCE_DIR"
    log_info "Threads: $THREADS, RAM: ${RAM}MB"
    log_info "C++ Standard: $CPP_STANDARD"
    log_info "Build Mode: $BUILD_MODE"
    log_info "Exclude Tests: $EXCLUDE_TESTS"

    parse_args "$@"
    validate_inputs
    clean_database
    create_database
    run_analysis
    process_results

    local exit_code=$?

    if [[ $exit_code -eq 0 ]]; then
        log_success "🎉 Analysis completed successfully with no critical issues!"
    elif [[ $exit_code -eq 2 ]]; then
        log_warning "⚠️  Analysis completed but found critical/high severity issues"
    else
        log_error "❌ Analysis failed"
    fi

    log_info "Results available in: $OUTPUT_DIR"

    exit $exit_code
}

# Execute main function with all arguments
main "$@"
