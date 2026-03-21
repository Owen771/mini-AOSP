#!/usr/bin/env bash
# mini-AOSP environment setup — installs build dependencies
# Idempotent: safe to run multiple times
set -euo pipefail

echo "=== mini-AOSP Environment Setup ==="

check_cmd() {
    command -v "$1" &>/dev/null
}

install_needed=()

# Check for C++ compiler
if check_cmd g++; then
    echo "✓ g++ found: $(g++ --version | head -1)"
elif check_cmd clang++; then
    echo "✓ clang++ found: $(clang++ --version | head -1)"
else
    install_needed+=("c++")
    echo "✗ No C++ compiler found"
fi

# Check for Java 16+ (needed for Unix domain socket API)
if check_cmd java; then
    java_version=$(java -version 2>&1 | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    major=$(echo "$java_version" | cut -d. -f1)
    if [ "$major" -ge 16 ] 2>/dev/null; then
        echo "✓ Java found: $(java -version 2>&1 | head -1)"
    else
        install_needed+=("java16+")
        echo "✗ Java $major found but need 16+ for Unix domain sockets"
    fi
else
    install_needed+=("java")
    echo "✗ Java not found"
fi

# Check for Kotlin compiler
if check_cmd kotlinc; then
    echo "✓ kotlinc found: $(kotlinc -version 2>&1 | head -1)"
else
    install_needed+=("kotlin")
    echo "✗ kotlinc not found"
fi

# Check for make
if check_cmd make; then
    echo "✓ make found: $(make --version | head -1)"
else
    install_needed+=("make")
    echo "✗ make not found"
fi

if [ ${#install_needed[@]} -eq 0 ]; then
    echo ""
    echo "All dependencies satisfied!"
    exit 0
fi

echo ""
echo "Missing: ${install_needed[*]}"
echo ""

# Detect OS and install
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "=== macOS detected ==="
    if ! check_cmd brew; then
        echo "Please install Homebrew: https://brew.sh"
        exit 1
    fi

    for dep in "${install_needed[@]}"; do
        case "$dep" in
            "c++")
                echo "Installing: Xcode command line tools..."
                xcode-select --install 2>/dev/null || echo "  (already installed or installing)"
                ;;
            "java"|"java16+")
                echo "Installing: OpenJDK 17..."
                brew install openjdk@17
                echo "  You may need: sudo ln -sfn $(brew --prefix openjdk@17)/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-17.jdk"
                ;;
            "kotlin")
                echo "Installing: Kotlin..."
                brew install kotlin
                ;;
            "make")
                echo "make should be part of Xcode CLI tools"
                xcode-select --install 2>/dev/null || true
                ;;
        esac
    done

elif [[ -f /etc/os-release ]]; then
    . /etc/os-release
    echo "=== Linux detected: $NAME ==="

    case "$ID" in
        ubuntu|debian)
            sudo apt-get update -qq
            for dep in "${install_needed[@]}"; do
                case "$dep" in
                    "c++")     sudo apt-get install -y g++ ;;
                    "java"|"java16+") sudo apt-get install -y openjdk-17-jdk ;;
                    "kotlin")  sudo snap install --classic kotlin || echo "Install kotlin manually: https://kotlinlang.org/docs/command-line.html" ;;
                    "make")    sudo apt-get install -y make ;;
                esac
            done
            ;;
        fedora|rhel|centos)
            for dep in "${install_needed[@]}"; do
                case "$dep" in
                    "c++")     sudo dnf install -y gcc-c++ ;;
                    "java"|"java16+") sudo dnf install -y java-17-openjdk-devel ;;
                    "kotlin")  echo "Install kotlin via SDKMAN: curl -s https://get.sdkman.io | bash && sdk install kotlin" ;;
                    "make")    sudo dnf install -y make ;;
                esac
            done
            ;;
        *)
            echo "Unsupported Linux distro: $ID"
            echo "Please install manually: g++, openjdk-17, kotlin, make"
            exit 1
            ;;
    esac
else
    echo "Unsupported OS: $OSTYPE"
    exit 1
fi

echo ""
echo "=== Setup complete. Re-run to verify. ==="
