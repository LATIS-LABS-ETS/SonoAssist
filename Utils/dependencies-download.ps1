$url = "https://github.com/LATIS-ETS/SonoAssist/releases/download/1.0/Dependencies.zip"
$src_dir = "$(Split-Path -Path $PSScriptRoot -Parent)\SonoAssist"
$output = "$src_dir\Dependencies.zip"
$decompressedOutput = "$src_dir\Dependencies"
$debug_dir = "$src_dir\out\build\x64-Debug\bin"
$release_dir = "$src_dir\out\build\x64-Release\bin"

Write-Host "*****************************************"
Write-Host "Begin SonoAssist dependencies integration"
Write-Host "*****************************************"

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

Write-Host "Checking if Dependencies directory exists..."
if (Test-Path -Path $decompressedOutput) {
    "--> Directory exists."
} else {
    Write-Host "Retrieving dependencies..."
    Write-Host "--> This may take several minutes. "
    (New-Object System.Net.WebClient).DownloadFile($url, $output)

    Write-Host "Extracting dependencies..."
    Expand-7Zip -ArchiveFileName $output -TargetPath $src_dir
}

Write-Host "Copying debug dependencies..."
if (!(Test-Path -Path $debug_dir)) {
    New-Item -ItemType Directory -Force -Path $debug_dir
}
Copy-Item -Path $decompressedOutput\debug_dlls\* -Destination $debug_dir\ -PassThru -Recurse

Write-Host "Copying release dependencies..."
if (!(Test-Path -Path $release_dir)) {
    New-Item -ItemType Directory -Force -Path $release_dir
}
Copy-Item -Path $decompressedOutput\release_dlls\* -Destination $release_dir\ -PassThru -Recurse

Write-Host "Adding dependencies complete."
Write-Host "*****************************************"