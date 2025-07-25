# =============================================================================
# CodeQL Docker Container for C++ Analysis
# Optimized for LumexLib and similar C++ projects
# =============================================================================

FROM ubuntu:22.04 as base

# Metadata
LABEL maintainer="LumexLib Team"
LABEL description="CodeQL Container for C++ Security Analysis"
LABEL version="1.0.0"

# Build arguments
ARG CODEQL_VERSION="2.16.1"
ARG CODEQL_CLI_VERSION="2.16.1"
ARG DEBIAN_FRONTEND=noninteractive

# Environment variables
ENV CODEQL_HOME="/opt/codeql"
ENV CODEQL_CLI_HOME="/opt/codeql-cli"
ENV PATH="${CODEQL_CLI_HOME}:${CODEQL_HOME}:${PATH}"
ENV WORKDIR="/workspace"
ENV DATABASE_DIR="/tmp/codeql-db"
ENV OUTPUT_DIR="/tmp/results"

# =============================================================================
# SYSTEM DEPENDENCIES INSTALLATION
# =============================================================================

RUN apt-get update && apt-get install -y \
    # Build essentials for C++ compilation
    build-essential \
    gcc \
    g++ \
    clang \
    cmake \
    ninja-build \
    make \
    pkg-config \
    # CodeQL dependencies
    curl \
    wget \
    unzip \
    git \
    # Analysis tools
    jq \
    python3 \
    python3-pip \
    # Utilities
    vim \
    nano \
    tree \
    less \
    htop \
    # Cleanup
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && rm -rf /tmp/*

# =============================================================================
# CODEQL INSTALLATION
# =============================================================================

# Download and install CodeQL CLI
RUN mkdir -p ${CODEQL_CLI_HOME} && \
    cd /tmp && \
    wget -q "https://github.com/github/codeql-cli-binaries/releases/download/v${CODEQL_CLI_VERSION}/codeql-linux64.zip" && \
    unzip -q codeql-linux64.zip && \
    mv codeql/* ${CODEQL_CLI_HOME}/ && \
    chmod +x ${CODEQL_CLI_HOME}/codeql && \
    rm -rf /tmp/codeql*

# Download and install CodeQL queries
RUN mkdir -p ${CODEQL_HOME} && \
    cd /tmp && \
    wget -q "https://github.com/github/codeql/archive/refs/heads/main.zip" -O codeql-queries.zip && \
    unzip -q codeql-queries.zip && \
    mv codeql-main/* ${CODEQL_HOME}/ && \
    rm -rf /tmp/codeql*

# Verify CodeQL installation
RUN codeql version && \
    codeql resolve languages && \
    codeql resolve qlpacks

# =============================================================================
# WORKSPACE SETUP
# =============================================================================

# Create workspace directories
RUN mkdir -p ${WORKDIR} ${DATABASE_DIR} ${OUTPUT_DIR} && \
    chmod -R 755 ${WORKDIR} ${DATABASE_DIR} ${OUTPUT_DIR}

# Create non-root user for security
RUN groupadd -r codeql && \
    mkdir -p /home/codeql && \
    useradd -r -g codeql -d /home/codeql -s /bin/bash codeql && \
    chown -R codeql:codeql ${WORKDIR} ${DATABASE_DIR} ${OUTPUT_DIR} ${CODEQL_HOME} ${CODEQL_CLI_HOME} /home/codeql

# =============================================================================
# ANALYSIS SCRIPTS AND UTILITIES
# =============================================================================

# Copy analysis scripts
COPY scripts/ /usr/local/bin/
RUN chmod +x /usr/local/bin/*.sh

# =============================================================================
# BUILD INFORMATION
# =============================================================================

# Add build info (before switching to non-root user)
RUN echo "CodeQL Version: ${CODEQL_CLI_VERSION}" > /opt/build-info.txt && \
    echo "Build Date: $(date)" >> /opt/build-info.txt && \
    echo "Base Image: ubuntu:22.04" >> /opt/build-info.txt && \
    echo "Optimized for: C++ Analysis" >> /opt/build-info.txt

# =============================================================================
# ALIASES AND ENVIRONMENT SETUP
# =============================================================================

# Create helpful aliases and functions
RUN echo '#!/bin/bash' > /home/codeql/.bashrc && \
    echo 'export PS1="\[\033[01;32m\]codeql@\h\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$ "' >> /home/codeql/.bashrc && \
    echo '' >> /home/codeql/.bashrc && \
    echo '# CodeQL Aliases' >> /home/codeql/.bashrc && \
    echo 'alias cql="codeql"' >> /home/codeql/.bashrc && \
    echo 'alias cql-version="codeql version"' >> /home/codeql/.bashrc && \
    echo 'alias cql-languages="codeql resolve languages"' >> /home/codeql/.bashrc && \
    echo 'alias cql-packs="codeql resolve qlpacks"' >> /home/codeql/.bashrc && \
    echo 'alias ll="ls -alF"' >> /home/codeql/.bashrc && \
    echo 'alias la="ls -A"' >> /home/codeql/.bashrc && \
    echo 'alias l="ls -CF"' >> /home/codeql/.bashrc && \
    echo '' >> /home/codeql/.bashrc && \
    echo '# CodeQL Functions' >> /home/codeql/.bashrc && \
    echo 'cql-full-scan() {' >> /home/codeql/.bashrc && \
    echo '    full-scan.sh "$@"' >> /home/codeql/.bashrc && \
    echo '}' >> /home/codeql/.bashrc && \
    echo '' >> /home/codeql/.bashrc && \
    echo 'echo "🔍 CodeQL Container Ready!"' >> /home/codeql/.bashrc && \
    echo 'echo "📁 Workspace: ${WORKDIR}"' >> /home/codeql/.bashrc && \
    echo 'echo "🛠️  Available commands: cql-full-scan"' >> /home/codeql/.bashrc && \
    echo 'echo "📖 Run: full-scan.sh --help for usage"' >> /home/codeql/.bashrc && \
    chown codeql:codeql /home/codeql/.bashrc

# =============================================================================
# FINAL SETUP
# =============================================================================

# Switch to non-root user
USER codeql
WORKDIR ${WORKDIR}

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD codeql version || exit 1

# Default command
CMD ["/bin/bash"]
