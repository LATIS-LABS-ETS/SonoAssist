$url = "https://github.com/LATIS-ETS/SonoAssist/releases/download/0.0/Dependencies.zip"
$parent_dir = Split-Path -Path $PSScriptRoot -Parent
$output = "$parent_dir\Dependencies.7z"
$decompressedOutput = "$parent_dir\Dependencies"
$debug_dir = "$parent_dir\SonoAssist\x64\Debug"
$release_dir = "$parent_dir\SonoAssist\x64\Release"

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
    Expand-7Zip -ArchiveFileName $output -TargetPath $parent_dir
}



Write-Host "Copying debug dependencies..."

if (!(Test-Path -Path $debug_dir)) {
    New-Item -ItemType Directory -Force -Path $debug_dir
}

Copy-Item -Path "C:\Program Files (x86)\Intel RealSense SDK 2.0\bin\x64\Intel.Realsense.dll" -Destination $debug_dir\Intel.Realsense.dll
Copy-Item -Path $decompressedOutput\clarius_listener\lib\listen.dll -Destination $debug_dir\listen.dll
Copy-Item -Path $decompressedOutput\MetaWear-SDK-Cpp-0.18.4\dist\Release\lib\x64\MetaWear.Win32.dll -Destination $debug_dir\MetaWear.Win32.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_calib3d3410d.dll -Destination $debug_dir\opencv_calib3d3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_core3410d.dll -Destination $debug_dir\opencv_core3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_features2d3410d.dll -Destination $debug_dir\opencv_features2d3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_highgui3410d.dll -Destination $debug_dir\opencv_highgui3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_imgcodecs3410d.dll -Destination $debug_dir\opencv_imgcodecs3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_imgproc3410d.dll -Destination $debug_dir\opencv_imgproc3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_ml3410d.dll -Destination $debug_dir\opencv_ml3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_objdetect3410d.dll -Destination $debug_dir\opencv_objdetect3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_photo3410d.dll -Destination $debug_dir\opencv_photo3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_shape3410d.dll -Destination $debug_dir\opencv_shape3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_stitching3410d.dll -Destination $debug_dir\opencv_stitching3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_superres3410d.dll -Destination $debug_dir\opencv_superres3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_video3410d.dll -Destination $debug_dir\opencv_video3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_videoio3410d.dll -Destination $debug_dir\opencv_videoio3410d.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Debug\opencv_videostab3410d.dll -Destination $debug_dir\opencv_videostab3410d.dll
Copy-Item -Path "C:\Program Files (x86)\Intel RealSense SDK 2.0\bin\x64\realsense2.dll" -Destination $debug_dir\realsense2.dll
Copy-Item -Path $decompressedOutput\stream_engine_windows_x64_4.1.0.3\lib\tobii\tobii_stream_engine.dll -Destination $debug_dir\tobii_stream_engine.dll

Write-Host "Copying release dependencies..."

if (!(Test-Path -Path $release_dir)) {
    New-Item -ItemType Directory -Force -Path $release_dir
}

Copy-Item -Path "C:\Program Files (x86)\Intel RealSense SDK 2.0\bin\x64\Intel.Realsense.dll" -Destination $release_dir\Intel.Realsense.dll
Copy-Item -Path $decompressedOutput\clarius_listener\lib\listen.dll -Destination $release_dir\listen.dll
Copy-Item -Path $decompressedOutput\MetaWear-SDK-Cpp-0.18.4\dist\Release\lib\x64\MetaWear.Win32.dll -Destination $release_dir\MetaWear.Win32.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_calib3d3410.dll -Destination $release_dir\opencv_calib3d3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_core3410.dll -Destination $release_dir\opencv_core3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_features2d3410.dll -Destination $release_dir\opencv_features2d3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_highgui3410.dll -Destination $release_dir\opencv_highgui3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_imgcodecs3410.dll -Destination $release_dir\opencv_imgcodecs3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_imgproc3410.dll -Destination $release_dir\opencv_imgproc3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_ml3410.dll -Destination $release_dir\opencv_ml3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_objdetect3410.dll -Destination $release_dir\opencv_objdetect3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_photo3410.dll -Destination $release_dir\opencv_photo3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_shape3410.dll -Destination $release_dir\opencv_shape3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_stitching3410.dll -Destination $release_dir\opencv_stitching3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_superres3410.dll -Destination $release_dir\opencv_superres3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_video3410.dll -Destination $release_dir\opencv_video3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_videoio3410.dll -Destination $release_dir\opencv_videoio3410.dll
Copy-Item -Path $decompressedOutput\opencv-3.4.10\opencv-3.4.10\build\bin\Release\opencv_videostab3410.dll -Destination $release_dir\opencv_videostab3410.dll
Copy-Item -Path "C:\Program Files (x86)\Intel RealSense SDK 2.0\bin\x64\realsense2.dll" -Destination $release_dir\realsense2.dll
Copy-Item -Path $decompressedOutput\stream_engine_windows_x64_4.1.0.3\lib\tobii\tobii_stream_engine.dll -Destination $release_dir\tobii_stream_engine.dll

Write-Host "Adding dependencies complete."
Write-Host "*****************************************"