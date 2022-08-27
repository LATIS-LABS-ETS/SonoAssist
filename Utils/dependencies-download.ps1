$src_folder = "$(Split-Path -Path $PSScriptRoot -Parent)\SonoAssist"

$dependencies_url = "https://github.com/LATIS-ETS/SonoAssist/releases/download/1.0/Dependencies.zip"
$dependencies_archive = "$src_folder\Dependencies.zip"
$dependencies_folder = "$src_folder\Dependencies"

# this version of lib torch is (Release) build only, check : https://pytorch.org/
$libtorch_url = "https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-1.12.1%2Bcpu.zip"
$libtorch_archive = "$dependencies_folder\libtorch-win-shared-with-deps-1.12.1+cpu.zip"
$libtorch_folder = "$dependencies_folder\libtorch"

$debug_bin_dir = "$src_folder\out\build\x64-Debug\bin"
$release_bin_dir = "$src_folder\out\build\x64-Release\bin"

Write-Host "*****************************************"
Write-Host "Begin SonoAssist dependencies integration"
Write-Host "*****************************************"

# 7zip check / install
Write-Host "Checking if 7zip module is installed.."
if (Get-Module -ListAvailable -Name 7Zip4Powershell) {
    Write-Host "--> 7Zip4Powershell Already Installed. "
} 
else {
    try {
        Write-Host "Installing 7zip module for Powershell.."
        Install-Module -Name 7Zip4Powershell
    }
    catch [Exception] {
        $_.message 
        exit
    }
}

# fetching the dependencies folder from the latis github
Write-Host "Checking if Dependencies directory exists..."
if (Test-Path -Path $dependencies_folder) {
    Write-Host "--> Directory exists."
} else {
    Write-Host "Retrieving dependencies from : https://github.com/LATIS-ETS/SonoAssist"
    Write-Host "--> This may take several minutes. "
    (New-Object System.Net.WebClient).DownloadFile($dependencies_url, $dependencies_archive)

    Write-Host "Extracting dependencies..."
    Expand-7Zip -ArchiveFileName $dependencies_archive -TargetPath $src_folder
    Remove-Item $dependencies_archive
}

# fetching a libtorch release
Write-Host "Checking if libtorch directory exists..."
if (Test-Path -Path $libtorch_folder) {
    Write-Host "--> Directory exists."
} else {
    Write-Host "Retrieving a libtorch release"
    Write-Host "--> This may take several minutes."
    (New-Object System.Net.WebClient).DownloadFile($libtorch_url, $libtorch_archive)
    Write-Host "Extracting libtorch ..."
    Expand-7Zip -ArchiveFileName $libtorch_archive -TargetPath $dependencies_folder
    Remove-Item $libtorch_archive
}

# copying the required dlls to the release/debug bin folders

Write-Host "Copying debug dependencies..."
if (!(Test-Path -Path $debug_bin_dir)) {
    New-Item -ItemType Directory -Force -Path $debug_bin_dir
}
Copy-Item -Path $dependencies_folder\debug_dlls\* -Destination $debug_bin_dir\ -PassThru -Recurse

Write-Host "Copying release dependencies..."
if (!(Test-Path -Path $release_bin_dir)) {
    New-Item -ItemType Directory -Force -Path $release_bin_dir
}
Copy-Item -Path $dependencies_folder\release_dlls\* -Destination $release_bin_dir\ -PassThru -Recurse
Copy-Item -Path $libtorch_folder\lib\*.dll -Destination $release_bin_dir\ -PassThru

Write-Host "Adding dependencies complete."
Write-Host "*****************************************"