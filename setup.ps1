#!/usr/bin/env pwsh

# Setup script for Godot Extensions workspace
# Downloads dependencies and initializes git submodules

function Write-Step($message) {
    Write-Host "`n>>> $message" -ForegroundColor Cyan
}

function Write-Success($message) {
    Write-Host "✓ $message" -ForegroundColor Green
}

function Write-Error($message) {
    Write-Host "✗ $message" -ForegroundColor Red
}

# Check if we're in a git repository
if (-not (Test-Path ".git")) {
    Write-Error "Not a git repository. Please run this script from the repository root."
    exit 1
}

Write-Host "Godot Extensions - Setup Script" -ForegroundColor Magenta
Write-Host "================================`n"

# Initialize and update submodules
Write-Step "Initializing git submodules..."
try {
    git submodule update --init --recursive
    Write-Success "Git submodules initialized"
} catch {
    Write-Error "Failed to initialize submodules: $_"
    exit 1
}

# Verify godot-cpp
Write-Step "Verifying godot-cpp setup..."
if (Test-Path "godot-cpp/SConstruct") {
    Write-Success "godot-cpp found at ./godot-cpp/"
} else {
    Write-Error "godot-cpp not found. Submodule initialization may have failed."
    exit 1
}

# Check for extension source directories
Write-Step "Checking extensions..."
$extensions = @("midi_player", "dascript")
foreach ($ext in $extensions) {
    if (Test-Path "$ext/src") {
        Write-Success "$ext extension found"
    } elseif ($ext -eq "dascript") {
        Write-Host "  (optional, not yet set up)" -ForegroundColor Yellow
    } else {
        Write-Error "$ext extension not found"
        exit 1
    }
}

# Verify addons directory
Write-Step "Checking addons structure..."
if (Test-Path "addons/midi_player") {
    Write-Success "Shared addons directory found"
} else {
    Write-Error "Shared addons directory missing"
    exit 1
}

# Check for SCons
Write-Step "Checking SCons installation..."
try {
    $scons_version = scons --version 2>$null | Select-Object -First 1
    if ($LASTEXITCODE -eq 0) {
        Write-Success "SCons found: $scons_version"
    } else {
        Write-Host "SCons not found. Install with: pip install scons" -ForegroundColor Yellow
    }
} catch {
    Write-Host "SCons not found. Install with: pip install scons" -ForegroundColor Yellow
}

# Summary
Write-Host "`n================================" -ForegroundColor Magenta
Write-Host "Setup Complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Install SCons if not already done:"
Write-Host "   pip install scons"
Write-Host ""
Write-Host "2. Build the MIDI Player extension:"
Write-Host "   scons platform=windows target=template_debug arch=x86_64"
Write-Host ""
Write-Host "3. Or use the convenience script:"
Write-Host "   .\build.bat"
Write-Host ""
Write-Host "For more information, see BUILD.md"
